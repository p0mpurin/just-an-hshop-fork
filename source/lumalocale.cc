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

#include "lumalocale.hh"
#include "mng.hh"
#include "settings.hh"
#include "util.hh"
#include "log.hh"

#include <ui/smdhicon.hh>
#include <ui/selector.hh>
#include <ui/confirm.hh>
#include <ui/base.hh>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern bool ns_was_init;


static bool has_region(ctr::smdh::SMDH *smdh, ctr::smdh::Region region)
{
	// This is a mess
	return
		(smdh->region & (u32) ctr::smdh::RegionLockout::JPN && region == ctr::smdh::Region::JPN) ||
		(smdh->region & (u32) ctr::smdh::RegionLockout::USA && region == ctr::smdh::Region::USA) ||
		(smdh->region & (u32) ctr::smdh::RegionLockout::EUR && region == ctr::smdh::Region::EUR) ||
		(smdh->region & (u32) ctr::smdh::RegionLockout::AUS && region == ctr::smdh::Region::EUR) ||
		(smdh->region & (u32) ctr::smdh::RegionLockout::CHN && region == ctr::smdh::Region::CHN) ||
		(smdh->region & (u32) ctr::smdh::RegionLockout::KOR && region == ctr::smdh::Region::KOR) ||
		(smdh->region & (u32) ctr::smdh::RegionLockout::TWN && region == ctr::smdh::Region::TWN);
}

static const char *get_auto_lang_str(ctr::smdh::SMDH *smdh)
{
	if(smdh->region & (u32) ctr::smdh::RegionLockout::JPN) return "JP";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::USA) return "EN";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::EUR) return "EN";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::AUS) return "EN";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::CHN) return "ZH";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::KOR) return "KR";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::TWN) return "TW";
	// Fail
	return nullptr;
}

#define LANG_INVALID 12
static const char *get_manual_lang_str(ctr::smdh::SMDH *smdh)
{
	ctr::smdh::Title *title = ctr::smdh::get_native_title(smdh);
	bool focus = ui::set_focus(true);
	ui::I18NEnabledRenderQueue queue;

	u8 lang = LANG_INVALID;
	// EN, JP, FR, DE, IT, ES, ZH, KO, NL, PT, RU, TW
	static const std::vector<std::string> langlut = { "EN", "JP", "FR", "DE", "IT", "ES", "ZH", "KO", "NL", "PT", "RU", "TW" };
	static const std::vector<u8> enumVals = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

	ui::builder<ui::SMDHIcon>(ui::Screen::top, smdh, SMDHIconType::large)
		.x(ui::dimensions::width_top / 2 - 30)
		.y(ui::dimensions::height / 2 - 64)
		.border()
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top,
			u16conv(title->descShort, 0x40) + "\n" +
			u16conv(title->descLong, 0x80))
		.x(ui::layout::center_x)
		.under(queue.back())
		.wrap()
		.add_to(queue);
	ui::builder<ui::Selector<u8>>(ui::Screen::bottom, langlut, enumVals, &lang)
		.add_to(queue);

	queue.render_finite_button(KEY_B);

	ui::set_focus(focus);
	return lang == LANG_INVALID
		? nullptr : langlut[lang].c_str();
}
#undef LANG_INVALID

static const char *get_region_str(ctr::smdh::SMDH *smdh)
{
	if(smdh->region & (u32) ctr::smdh::RegionLockout::JPN) return "JPN";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::USA) return "USA";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::EUR) return "EUR";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::AUS) return "EUR";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::CHN) return "CHN";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::KOR) return "KOR";
	if(smdh->region & (u32) ctr::smdh::RegionLockout::TWN) return "TWN";
	// Fail
	return nullptr;
}

static std::string ensure_path(ctr::title_id tid)
{
	// path: /luma/titles/{tid}/locale.txt
	std::string path;
#define ITER(x) do { path += (x); mkdir(path.c_str(), 0777); } while(0)
	ITER("/luma");
	ITER("/titles/");
	ITER(tid.to_string());
#undef ITER
	return path + "/locale.txt";
}

static void write_file(ctr::title_id tid, const char *region, const char *lang)
{
	std::string path = ensure_path(tid);
	// locale.txt already exists
	if(access(path.c_str(), F_OK) == 0)
		return;

	FILE *file = fopen(path.c_str(), "w");
	if(file == nullptr) return;
	fprintf(file, "%s %s", region, lang);
	fclose(file);
}

static bool enable_gamepatching_buf_legacy(u8 *buf)
{
	// Luma3DS config format
	// byte 0-3 : "CONF"
	// byte 4-5 : version_major
	// byte 6-7 : version_minor
	// byte ... : config

	/* is this actually a luma configuration file? */
	if(memcmp(buf, "CONF", 4) != 0)
		return true;

	// version_major != 2
	if(* (u16 *) &buf[4] != 2)
		return true;

	bool ret = buf[8] & 0x8;
	buf[8] |= 0x8;
	return ret;
}

/* returns if a reboot is required */
static bool enable_gamepatching_legacy()
{
	FILE *config = fopen("/luma/config.bin", "r");
	if(config == nullptr) return false;

	u8 buf[33];
	/* if the file is not the correct size abort */
	if(fread(buf, 1, 33, config) != 32)
	{ fclose(config); return false; }

	fclose(config);
	vlog("checking if gamepatching is enabled");
	bool isSet = enable_gamepatching_buf_legacy(buf);
	vlog("isSet: %i", isSet);
	if(isSet) return false;

	// We need to update settings
	config = fopen("/luma/config.bin", "w");
	if(config == nullptr) return true;

	if(fwrite(buf, 1, 32, config) != 32)
	{ fclose(config); return false; }

	ilog("successfully enabled game patching.");

	fclose(config);
	return true;
}

static int enable_gamepatching_new_f(FILE *config)
{
	/* Don't waste time properly parsing ini file.  Instead, search for the
	 * setting with simple string comparisons.  This won't cover edge cases
	 * where the user has changed the file manually and added or removed
	 * some whitespace, but in this situation it would be reasonable to
	 * assume that they know what they are doing and can manage on their
	 * own. */
	char buf[64];
	static constexpr const char search_str[] = "enable_game_patching = ";
	static constexpr const size_t search_str_len = sizeof(search_str) - 1;

	while(fgets(buf, sizeof(buf), config) != nullptr)
	{
		bool line_too_long = false;
		size_t len;
		while((len = strlen(buf)) == sizeof(buf) - 1 && buf[sizeof(buf) - 2] != '\n')
		{
			line_too_long = true;
			if(fgets(buf, sizeof(buf), config) == nullptr)
				return 0;
		}
		if(line_too_long) continue;

		if(len == search_str_len + 2 && memcmp(buf, search_str, search_str_len) == 0)
		{
			if(buf[search_str_len] == '0')
			{
				if(fseek(config, -2, SEEK_CUR) != 0) return -1;
				if(fputc('1', config) == EOF) return -1;
				ilog("game patching successfully enabled");
				return 1;
			}
			else if(buf[search_str_len] == '1')
			{
				vlog("game patching is already enabled");
				return 0;
			}
		}
	}

	vlog("could not determine if game patching is enabled");
	return false;
}

static bool enable_gamepatching_new()
{
	FILE *f = fopen("/luma/config.ini", "r+");
	if(!f) return false;
	int res = enable_gamepatching_new_f(f);
	fclose(f);
	if(res == -1)
		vlog("I/O error while trying to write /luma/config.ini");
	return res == 1;
}

/* returns if a reboot is required */
static bool enable_gamepatching()
{
	bool ret = false;
	if(access("/luma/config.bin", R_OK | W_OK) == 0)
		ret |= enable_gamepatching_legacy();
	if(access("/luma/config.ini", R_OK | W_OK) == 0)
		ret |= enable_gamepatching_new();
	return ret;
}

void luma::maybe_set_gamepatching()
{
	if(SETTING_LUMALOCALE == LumaLocaleMode::disabled)
		return;

	// We should prompt for reboot...
	if(enable_gamepatching())
	{
		if(R_SUCCEEDED(nsInit()))
		{
			if(ui::Confirm::exec(str::reboot_now, str::patching_reboot))
			{
				NS_RebootSystem();
				nsExit(); /* just in case this actually matters */
			}
		}
	}
}

bool luma::set_locale(ctr::title_id tid, bool interactive)
{
	// we don't want to set a locale
	LumaLocaleMode mode = SETTING_LUMALOCALE;
	if(mode == LumaLocaleMode::disabled || (!interactive && mode == LumaLocaleMode::manual))
		return false;

	ctr::smdh::SMDH *smdh = ctr::smdh::get(tid);
	ctr::smdh::Region region = ctr::smdh::Region::WORLD;
	const char *langstr = nullptr;
	const char *regstr = nullptr;

	if(smdh == nullptr)
		return false;
	bool ret = false;

	// We don't need to do anything
	if(smdh->region == (u32) ctr::smdh::RegionLockout::WORLD)
		goto del_smdh;

	// Convert to Region
	switch(ctr::mng::get_system_region())
	{
		case CFG_REGION_AUS: case CFG_REGION_EUR: region = ctr::smdh::Region::EUR; break;
		case CFG_REGION_CHN: region = ctr::smdh::Region::CHN; break;
		case CFG_REGION_JPN: region = ctr::smdh::Region::JPN; break;
		case CFG_REGION_KOR: region = ctr::smdh::Region::KOR; break;
		case CFG_REGION_TWN: region = ctr::smdh::Region::TWN; break;
		case CFG_REGION_USA: region = ctr::smdh::Region::USA; break;
		default: goto del_smdh; // invalid region
	}

	// If we have our own region we don't need to do anything
	if(has_region(smdh, region)) goto del_smdh;
	regstr = get_region_str(smdh);

	if(mode == LumaLocaleMode::automatic)
	{
		langstr = get_auto_lang_str(smdh);
		if(!langstr) goto del_smdh; /* shouldn't happen */
		ilog("(lumalocale) Automatically deduced %s %s", regstr, langstr);
	}
	else if(mode == LumaLocaleMode::manual)
	{
		langstr = get_manual_lang_str(smdh);
		/* cancelled the selection */
		if(!langstr) goto del_smdh;
		ilog("(lumalocale) Manually selected %s %s", regstr, langstr);
	}

	write_file(tid, regstr, langstr);
	ret = true;

del_smdh:
	delete smdh;
	return ret;
}

