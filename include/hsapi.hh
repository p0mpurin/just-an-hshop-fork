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

#ifndef inc_hsapi_hh
#define inc_hsapi_hh

#include <nblib/nblib.hh>

#include <unordered_map>
#include <string>
#include <vector>

#include <3ds.h>

#include <ui/confirm.hh>
#include <ui/loading.hh>
#include <ui/base.hh>

#include "panic.hh"
#include "error.hh"
#include "titleid.hh"

#define REGION_USA      "north-america"
#define REGION_EUROPE   "europe"
#define REGION_JAPAN    "japan"
#define THEMES_CATEGORY "themes"


namespace hsapi
{
	/* base types */
	using hsize      = uint64_t; /* size type */
	using hiver      = uint16_t; /* integer version type */
	using htid       = ctr::title_id; /* title id type */
	using hid        = uint32_t; /* landing id type */
	using hflags     = uint64_t; /* flag */
	using hprio      = uint8_t;  /* priority */
	using hcid       = uint8_t;  /* (sub)category id */

	namespace TitleFlag
	{
		enum TitleFlag {
			is_ktr           = 1 << 0,
			locale_emulation = 1 << 1,
			installer        = 1 << 2,
			physical_only    = 1 << 3,
			skip_force_rel   = 1 << 4,
		};
	}

	namespace VCType
	{
		enum VCFlag {
			none     = 0,
			gb       = 1,
			gbc      = 2,
			gba      = 3,
			nes      = 4,
			snes     = 5,
			gamegear = 6,
			pcengine = 7
		};
		constexpr int shift = 59;
		constexpr int mask  = 7;
	}


	/* copied NB types */
	using IndexSubcategory = nb::IndexSubcategory;
	using IndexCategory = nb::IndexCategory;

	static_assert(sizeof(ctr::title_id) == 8, "improper ctr::title_id size");

	using PartialTitle = nb::SimpleTitle<ctr::title_id>;
	using Title = nb::Title<ctr::title_id>;
	using RelatedFullTitle = nb::RelatedTitle<hsapi::Title>;
	using IndexMeta = nb::NbIndexMeta;
	using IdPair = nb::IdPair;

	using Subcategory = IndexSubcategory;
	using Category = IndexCategory;

	using SubcategoryMap = std::map<hcid, Subcategory>;
	using CategoryMap = std::map<hcid, Category>;

	using ErrorNamespace = nb::ResultStatusCode::Namespace;
	using ErrorReason = nb::ResultStatusCode::Reason;
	using Error = nb::ResultStatusCode;

	using TitleRelationType = nb::RelationType;

	/* index ops */
	void sorted_categories(std::vector<hsapi::Category>& categories);
	IndexSubcategory& subcategory(hcid cid, hcid sid);
	IndexCategory& category(hcid cid);
	CategoryMap& categories();
	Result fetch_index();
	IndexMeta& imeta();

	/* title ops */
	Result search(std::vector<PartialTitle>& ret, const std::unordered_map<std::string, std::string>& params);
	Result titles_in(std::vector<PartialTitle>& ret, const IndexCategory& cat, const IndexSubcategory& scat);
	Result get_by_title_id(std::vector<Title>& ret, const std::string& title_id);
	Result single_relations(std::vector<hsapi::RelatedFullTitle>& ret, hsapi::hid id);
	Result multiple_relations(std::vector<hsapi::RelatedFullTitle>& ret, const std::vector<hsapi::hid>& ids);
	Result batch_related(std::vector<Title>& ret, const std::vector<hsapi::htid>& tids);
	Result id_pair_by_id(std::vector<IdPair>& ret, const std::vector<hsapi::hid>& ids);
	Result id_pair_by_title_id(std::vector<IdPair>& ret, const std::vector<hsapi::htid>& ids);
	Result title_meta(Title& ret, hid id);
	Result random(Title& ret);

	Result get_by_id(hsapi::Title &ret, const hsapi::hid id);
	Result get_by_ids(std::vector<hsapi::Title>& ret, const std::vector<hsapi::hid>& ids);

	/* misc. api */
	Result upload_log(const char *contents, u32 size, std::string& logid);
	Result get_theme_preview_png(std::string& ret, hsapi::hid id);
	Result get_latest_version_string(std::string& ret);
	Result get_nocturne_latest_version_string(std::string& ret);

	/* dlapi ops */
	Result get_download_link(std::string& ret, hid id);
	Result get_download_link(std::string& ret, const Title& meta);

	hsapi::Error& last_error();
	Result handle_nb_result(nb::Result& nres);

	/* offline string construction */
	std::string format_category_and_subcategory(hcid cid, hcid sid);
	std::string update_location(const std::string& ver);
	std::string parse_vstring(hiver version);

	std::string title_name(const hsapi::PartialTitle& title);
	std::string title_name(const hsapi::Title& title);

	// Silent call. ui::loading() is not called and it will stop after 3 tries
	// NOTE: You have to std::move() primitives (hid, hiver, htid, ...)
	template <typename ... Ts>
	Result scall(Result (*func)(Ts...), Ts&& ... args)
	{
		Result res = 0;
		int tries = 0;

		do {
			res = (*func)(args...);
			++tries;
		} while(R_FAILED(res) && res != APPERR_CANCELLED && tries < 3);
		return res;
	}

	// NOTE: You have to std::move() primitives (hid, hiver, htid, ...)
	// for the error intercept, true bypasses the internal loop
	template <typename ... Ts>
	Result call(std::function<bool(Result)> err_intercept, Result (*func)(Ts...), Ts&& ... args)
	{
		ui::prev_desc desc = ui::set_desc(str::loading);
		bool focus = ui::set_focus(false);
		Result res;
		do {
			ui::loading([&res, func, &args...]() -> void {
				res = (*func)(args...);
			});

			if(res == APPERR_CANCELLED)
				break; /* done already */
			if(R_FAILED(res)) // Ask if we want to retry
			{
				if (err_intercept && err_intercept(res))
					break;
				error_container err = get_error(res);
				report_error(err);
				handle_error(err);

				ui::I18NEnabledRenderQueue queue;
				bool cont = true;

				ui::builder<ui::Confirm>(ui::Screen::bottom, str::retry_req, cont)
					.y(ui::layout::center_y)
					.add_to(queue);

				queue.render_finite();

				if(!cont) break;
			}
		} while(R_FAILED(res));
		ui::set_focus(focus);
		ui::set_desc(desc);
		return res;
	}

	template <typename ... Ts>
	Result call(Result (*func)(Ts...), Ts&& ... args)
	{
		return call(std::function<bool(Result)>(), func, std::forward<Ts>(args)...);
	}
}

#endif
