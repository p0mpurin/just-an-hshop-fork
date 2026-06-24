/* This file is part of 3hs
 * Copyright (C) 2021-2025 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */
/* vim: foldmethod=marker */

#include "httpclient.hh"
#include "hsapi.hh"
#include "error.hh"
#include "settings.hh"
#include "util.hh"
#include "log.hh"

#include <nblib/nblib.hh>

#include <algorithm>

#if defined(HS_DEBUG_SERVER)
	#define HS_NB_BASE  HS_DEBUG_SERVER ":5000/nbapi"
	#define HS_CDN_BASE HS_DEBUG_SERVER ":5001"
	#define HS_SITE_LOC HS_DEBUG_SERVER ":5002"
#endif
#if !defined(HS_CDN_BASE) || !defined(HS_SITE_LOC) || !defined(HS_UPDATE_BASE) || !defined(HS_NB_BASE)
	#error "You must define HS_CDN_BASE, HS_SITE_LOC, HS_NB_BASE and HS_UPDATE_BASE"
#endif
#ifndef NOCTURNE_UPDATE_BASE
	#define NOCTURNE_UPDATE_BASE "https://github.com/p0mpurin/just-an-hshop-fork/releases/latest/download"
#endif

#define OK 0

static hsapi::Error last_err(0);

hsapi::Error& hsapi::last_error() { return last_err; }

/* {{{1 Network primitives */

static Result basereq(const std::string& url, std::string& data, HTTPC_RequestMethod reqmeth = HTTPC_METHOD_GET, const char *postdata = nullptr, u32 postdata_len = 0, bool vercheck_enabled = true, bool devauth_enabled = false)
{
	http::ResumableDownload downloader(true, vercheck_enabled, devauth_enabled);
	downloader.set_postdata(postdata, postdata_len);
	downloader.set_target(url, reqmeth);
	downloader.requires_authentication();

	downloader.on_total_size_try_get(
		[&]() -> Result {
			if(downloader.maybe_total_size())
				data.reserve(downloader.maybe_total_size());
			return 0;
		}
	);

	downloader.on_chunk([&](size_t chunk_size) -> Result {
		data.append(downloader.data_buffer<char>(), chunk_size);
		return 0;
	});

	Result res = downloader.execute_once();
	return res;
}

Result hsapi::handle_nb_result(nb::Result& nres)
{
	elog("Failed with Result of: %s (%u)", nres.message.c_str(), nres.code);
	last_err = nres.code;
	return APPERR_API_FAIL;
}

template <typename T, nb::StatusCode (*parse_func)(T&, u8 *, size_t) = nb::single_object::parse<T>>
static Result nbreq(const std::string& url, T& obj, HTTPC_RequestMethod reqm = HTTPC_METHOD_GET, const char *postdata = nullptr, u32 size = 0, bool use_dev_auth = false)
{
	std::string data;
	Result res = basereq(url, data, reqm, postdata, size, true, use_dev_auth);
	if(R_FAILED(res) && res != APPERR_NON200) return res;

	/* nblib requires its input pointer to be 4-byte aligned. std::string does
	 * not make that promise, even though it commonly happens to be aligned.
	 * Copying into a u32-backed buffer makes the requirement explicit and
	 * prevents hardware-only UNALIGNED parse failures. */
	std::vector<u32> aligned_data((data.size() + sizeof(u32) - 1) / sizeof(u32), 0);
	if(data.size())
		memcpy(aligned_data.data(), data.data(), data.size());
	u8 *nb_data = reinterpret_cast<u8 *>(aligned_data.data());

	ilog("Got API data: size=%lu magic=%02X%02X%02X%02X",
		(unsigned long)data.size(),
		data.size() > 0 ? nb_data[0] : 0,
		data.size() > 1 ? nb_data[1] : 0,
		data.size() > 2 ? nb_data[2] : 0,
		data.size() > 3 ? nb_data[3] : 0);

	nb::StatusCode sc = parse_func(obj, nb_data, data.size());

	switch(sc)
	{
	case nb::StatusCode::SUCCESS:
		dlog("nb parse: success");
		last_err = 0;
		break;
	case nb::StatusCode::NO_INPUT_DATA:
		elog("nb parse: no input data");
		break;
	case nb::StatusCode::MAGIC_MISMATCH:
		elog("nb parse: magic mismatch");
		break;
	case nb::StatusCode::INPUT_DATA_TOO_SHORT:
		elog("nb parse: input data too short");
		break;
	case nb::StatusCode::UNALIGNED:
		elog("nb parse: data unaligned");
		break;
	}

	if(sc == nb::StatusCode::MAGIC_MISMATCH)
	{
		/* We may have an nb::Result */
		nb::Result nres;
		sc = nb::single_object::parse<nb::Result>(nres, nb_data, data.size());
		if(sc == nb::StatusCode::SUCCESS)
			res = hsapi::handle_nb_result(nres);
	}
	if(sc != nb::StatusCode::SUCCESS)
	{
		res = APPERR_INVALID_NB; /* TODO: Ideally this log should be a dlog() but hidden behind some runtime trigger... */
		elog("full failed nb data: %.*s", (int) data.size(), data.c_str());
	}

	return res;
}

template <typename T>
static Result nbreqa(const std::string& url, std::vector<T>& obj, HTTPC_RequestMethod reqm = HTTPC_METHOD_GET)
{
	return nbreq<std::vector<T>, nb::array::parse<T>>(url, obj, reqm);
}

// https://en.wikipedia.org/wiki/Percent-encoding
static std::string percent_encode(const std::string& str)
{
	std::string ret;
	ret.reserve(str.size() * 3);
	char hex[4];

	for(size_t i = 0; i < str.size(); ++i)
	{
		if((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z') || (str[i] >= '0' && str[i] <= '9')
			|| str[i] == '.' || str[i] == '-' || str[i] == '_' || str[i] == '~')
			ret += str[i];
		else
		{
			snprintf(hex, 4, "%%%02X", str[i]);
			ret += hex;
		}
	}

	return ret;
}

static std::string make_query_string_url(const std::string& base, const std::unordered_map<std::string, std::string>& params)
{
	std::string ret = base;
	bool first_set = false;

	for(std::unordered_map<std::string, std::string>::const_iterator i = params.begin(); i != params.end(); ++i)
	{
		if(first_set)
			ret += "&" + i->first + "=" + percent_encode(i->second);
		else
		{
			ret += "?" + i->first + "=" + percent_encode(i->second);
			first_set = true;
		}
	}

	return ret;
}

/* 1}}} */

/* {{{1 Index ops */

namespace hsapi { using Index = nb::Index; }

static hsapi::Index g_index;
#if !RELEASE
static bool g_indexloaded = false;
#endif

static hsapi::Index& hindex()
{
#if !RELEASE
	if(!g_indexloaded)
		panic_assert(R_SUCCEEDED(hsapi::fetch_index()), "failed to fetch unloaded index (debug)");
#endif
	return g_index;
}

Result hsapi::fetch_index()
{
	Result res = nbreq<hsapi::Index>(HS_NB_BASE "/title-index", g_index);
#if !RELEASE
	if(R_SUCCEEDED(res))
		g_indexloaded = true;
#endif
	return res;
}

hsapi::IndexSubcategory& hsapi::subcategory(hcid cid, hcid sid)
{
	hsapi::IndexCategory& cat = category(cid);
	auto it = cat.subcategories.find(sid);
	if(it == cat.subcategories.end())
		panic("invalid subcategory id");
	return it->second;
}

hsapi::IndexCategory& hsapi::category(hcid cid)
{
	hsapi::Index& dx = hindex();
	auto it = dx.categories.find(cid);
	if(it == dx.categories.end())
		panic("invalid category id");
	return it->second;
}

void hsapi::sorted_categories(std::vector<hsapi::Category>& categories)
{
	hsapi::Index& dx = hindex();
	categories.reserve(dx.categories.size());
	for(auto it = dx.categories.begin(); it != dx.categories.end(); ++it)
		categories.push_back(it->second);
	std::sort(categories.begin(), categories.end(), [](const hsapi::Category& lhs, const hsapi::Category& rhs) -> bool {
		return lhs.prio < rhs.prio;
	});
}

hsapi::CategoryMap& hsapi::categories() { return hindex().categories; }
hsapi::IndexMeta& hsapi::imeta() { return hindex().meta; }

/* 1}}} */

/* {{{1 Title API */

Result hsapi::titles_in(std::vector<hsapi::PartialTitle>& ret, const hsapi::IndexCategory& cat, const hsapi::IndexSubcategory& scat)
{
	ilog("Listing titles in subcategory");
	return nbreqa<hsapi::PartialTitle>(HS_NB_BASE "/title/category/" + cat.name + "/" + scat.name, ret);
}

Result hsapi::title_meta(hsapi::Title& ret, hsapi::hid id)
{
	ilog("Transforming PartialTitle into Title");
	return nbreq<hsapi::Title>(HS_NB_BASE "/title/" + std::to_string(id), ret);
}

Result hsapi::search(std::vector<hsapi::PartialTitle>& ret, const std::unordered_map<std::string, std::string>& params)
{
	ilog("Performing search action");
	return nbreqa<hsapi::PartialTitle>(make_query_string_url(HS_NB_BASE "/title/search", params), ret);
}

Result hsapi::random(hsapi::Title& ret)
{
	ilog("Getting random title");
	return nbreq<hsapi::Title>(HS_NB_BASE "/title/random", ret);
}

/* new relations api */
Result hsapi::single_relations(std::vector<hsapi::RelatedFullTitle>& ret, hsapi::hid id)
{
	ilog("getting single title's relations");
	return nbreqa<hsapi::RelatedFullTitle>(HS_NB_BASE "/title/" + std::to_string(id) + "/relations", ret);
}

Result hsapi::multiple_relations(std::vector<hsapi::RelatedFullTitle>& ret, const std::vector<hsapi::hid>& ids)
{
	ilog("getting multiple title relations");
	std::string url = HS_NB_BASE "/title/relations/batch?ids=";
	join_t<hsapi::hid>(url, ids, ",");
	return nbreqa<hsapi::RelatedFullTitle>(url, ret);
}

/* legacy */
Result hsapi::batch_related(std::vector<hsapi::Title>& ret, const std::vector<hsapi::htid>& tids)
{
	ilog("Doing batch related query");
	if(tids.size() == 0)
		return OK;

	std::string url = HS_NB_BASE "/title/related/batch?title_ids=";
	join_t<ctr::title_id>(url, tids, ",", [](const ctr::title_id &tid) -> std::string { return tid.to_string(); });

	return nbreqa<hsapi::Title>(url, ret);
}

Result hsapi::id_pair_by_id(std::vector<hsapi::IdPair>& ret, const std::vector<hsapi::hid>& ids)
{
	ilog("Getting ID pairs by IDs");
	std::string url = HS_NB_BASE "/title/id-pair?ids=";
	join_t<hsapi::hid>(url, ids, ",");
	return nbreqa<hsapi::IdPair>(url, ret);
}

Result hsapi::id_pair_by_title_id(std::vector<hsapi::IdPair>& ret, const std::vector<hsapi::htid>& tids)
{
	ilog("Getting ID pairs by Title IDs");
	std::string url = HS_NB_BASE "/title/id-pair?title_ids=";
	join_t<ctr::title_id>(url, tids, ",", [](const ctr::title_id &tid) -> std::string { return tid.to_string(); });
	return nbreqa<hsapi::IdPair>(url, ret);
}

Result hsapi::get_by_title_id(std::vector<hsapi::Title>& ret, const std::string& title_id)
{
	ilog("Getting hshop title by title id");
	return nbreqa<hsapi::Title>(HS_NB_BASE "/title/id/" + title_id, ret);
}

Result hsapi::get_by_id(hsapi::Title &ret, const hsapi::hid id)
{
	ilog("Getting hshop title by id");
	return nbreq(HS_NB_BASE "/title/" + std::to_string(id), ret);
}

Result hsapi::get_by_ids(std::vector<hsapi::Title>& ret, const std::vector<hsapi::hid>& ids)
{
	ilog("Getting hshop titles by id array");
	std::string url = HS_NB_BASE "/title/batch?ids=";
	join_t<hsapi::hid>(url, ids, ",");
	return nbreqa<hsapi::Title>(url, ret);
}

/* 1}}} */

/* {{{1 Misc. API */

Result hsapi::upload_log(const char *contents, u32 size, std::string& logid)
{
	ilog("Uploading log");
	nb::ThsLogResult logres;
	Result res = nbreq<nb::ThsLogResult>(HS_NB_BASE "/3hs-log", logres, HTTPC_METHOD_POST, contents, size);
	if(R_FAILED(res)) return res;
	char hex[9];
	snprintf(hex, 9, "%08X", logres.id);
	logid = hex;
	return OK;
}

Result hsapi::get_latest_version_string(std::string& ret)
{
	ilog("Getting latest version");
	Result res = basereq(HS_UPDATE_BASE "/version", ret, HTTPC_METHOD_GET, nullptr, 0, false);
	if(R_FAILED(res)) return res;
	trim(ret, " \t\n");
	return OK;
}

Result hsapi::get_nocturne_latest_version_string(std::string& ret)
{
	ilog("Getting latest Nocturne version");
	Result res = basereq(NOCTURNE_UPDATE_BASE "/version.txt",
		ret, HTTPC_METHOD_GET, nullptr, 0, false);
	if(R_FAILED(res)) return res;
	trim(ret, " \t\n");
	return OK;
}

Result hsapi::get_theme_preview_png(std::string& ret, hsapi::hid id)
{
	ilog("Getting theme preview");
	Result res = basereq(HS_NB_BASE "/title/" + std::to_string(id) + "/theme-preview", ret);
	if(R_FAILED(res)) return res;
	nb::Result nres;
	if(nb::single_object::parse<nb::Result>(nres, (u8 *) ret.c_str(), ret.size()) == nb::StatusCode::SUCCESS)
		return hsapi::handle_nb_result(nres);
	return OK;
}

/* 1}}} */

/* {{{1 DLApi */

Result hsapi::get_download_link(std::string& ret, hid id)
{
	std::string data;
	Result res = basereq(HS_CDN_BASE "/nbcontent/" + std::to_string(id) + "/request",
		data, HTTPC_METHOD_GET, nullptr, 0, true, true);
	if(R_FAILED(res)) return res;

	/* TOKN is a single NB object with a fixed 16-byte object header,
	 * a 16-byte token header, and a blob containing the token string.
	 * Decode it explicitly here: this is the only endpoint required before
	 * every download, and the old generic decoder can disagree with the
	 * server layout because of target-specific structure padding. */
	if(data.size() < 32 || memcmp(data.data(), "TOKN", 4) != 0)
	{
		/* Preserve structured API errors when the CDN provides one. */
		std::vector<u32> aligned((data.size() + 3) / 4, 0);
		if(data.size()) memcpy(aligned.data(), data.data(), data.size());
		nb::Result nres;
		if(nb::single_object::parse<nb::Result>(nres,
			reinterpret_cast<u8 *>(aligned.data()), data.size()) == nb::StatusCode::SUCCESS)
			return hsapi::handle_nb_result(nres);
		return APPERR_INVALID_NB;
	}

	auto read_u32 = [&data](size_t offset) -> u32 {
		u32 value = 0;
		if(offset + sizeof(value) <= data.size())
			memcpy(&value, data.data() + offset, sizeof(value));
		return value;
	};

	u32 object_header_size = read_u32(4);
	u32 token_header_size  = read_u32(8);
	u32 blob_size          = read_u32(12);
	if(object_header_size != 16 || token_header_size < 16
		|| (u64)object_header_size + token_header_size + blob_size > data.size())
		return APPERR_INVALID_NB;

	size_t token_header = object_header_size;
	u32 response_id = read_u32(token_header + 8);
	u32 token_offset = read_u32(token_header + 12);
	size_t blob_start = object_header_size + token_header_size;
	if(response_id != id || token_offset >= blob_size)
		return APPERR_INVALID_NB;

	const char *token_start = data.data() + blob_start + token_offset;
	size_t token_room = blob_size - token_offset;
	size_t token_length = strnlen(token_start, token_room);
	if(token_length == 0 || token_length == token_room)
		return APPERR_INVALID_NB;

	std::string token(token_start, token_length);
	ret = HS_CDN_BASE "/nbcontent/" + std::to_string(id) + "?token=" + token;
	return OK;
}

Result hsapi::get_download_link(std::string& ret, const Title& meta)
{
	return get_download_link(ret, meta.id);
}

/* 1}}} */

/* {{{1 Misc. */

std::string hsapi::update_location(const std::string& ver)
{
#ifdef DEVICE_ID
#define STRING_(id) #id
#define STRINGIFY(id) STRING_(id)
	return HS_UPDATE_BASE "/3hs-" + ver + "-" STRINGIFY(DEVICE_ID) ".cia";
#undef STRING_
#undef STRINGIFY
#else
	return HS_UPDATE_BASE "/3hs-" + ver + ".cia";
#endif
}

std::string hsapi::parse_vstring(hsapi::hiver version)
{
	// based on:
	//  "{(ver >> 10) & 0x3F}.{(ver >> 4) & 0x3F}.{ver & 0xF}"
	return "v"
		+ std::to_string(version >> 10 & 0x3F) + "."
		+ std::to_string(version >> 4  & 0x3F) + "."
		+ std::to_string(version       & 0xF );
}

std::string hsapi::format_category_and_subcategory(hsapi::hcid cid, hsapi::hcid sid)
{
	hsapi::Category& cat = hsapi::category(cid);
	hsapi::Subcategory& scat = hsapi::subcategory(cid, sid);
	return cat.disp + " -> " + scat.disp;
}

template <typename T>
static std::string real_title_name(const T& title)
{
	const std::string& base = ISET_SHOW_ALT ? (title.alt.size() ? title.alt : title.name) : title.name;

	const char *vc_type;
	switch((title.flags >> hsapi::VCType::shift) & hsapi::VCType::mask)
	{
	case hsapi::VCType::gb: vc_type = "[GB] "; break;
	case hsapi::VCType::gbc: vc_type = "[GBC] "; break;
	case hsapi::VCType::gba: vc_type = "[GBA] "; break;
	case hsapi::VCType::nes: vc_type = hsapi::subcategory(title.cat, title.subcat).name == REGION_JAPAN ? "[Famicom] " : "[NES] "; break;
	case hsapi::VCType::snes: vc_type = hsapi::subcategory(title.cat, title.subcat).name == REGION_JAPAN ? "[Super Famicom] " : "[SNES] "; break;
	case hsapi::VCType::gamegear: vc_type = "[GameGear] "; break;
	case hsapi::VCType::pcengine: vc_type = hsapi::subcategory(title.cat, title.subcat).name == REGION_USA ? "[TurboGrafx-16] " : "[PC Engine] "; break;
	case hsapi::VCType::none:
	default:
		vc_type = nullptr;
		break;
	}

	return vc_type ? vc_type + base : base;
}

std::string hsapi::title_name(const hsapi::PartialTitle& title) { return real_title_name(title); }
std::string hsapi::title_name(const hsapi::Title& title)        { return real_title_name(title); }

/* 1}}} */
