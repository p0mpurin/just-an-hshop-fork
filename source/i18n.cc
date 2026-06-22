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

#include <ui/base.hh> /* for UI_GLYPH_* */

#include "settings.hh"
#include "panic.hh"
#include "i18n.hh"

#include <3ds.h>

#include "i18n_tab.cc"

const char *i18n::getstr(str::type id)
{
	I18NStringStore *store = RAW(get_nsettings()->lang, id);
	panic_assert(!(store->info & INFO_ISFUNC), "attempt to get parameter-less function");
	return store->string;
}

const char *i18n::getstr(str::type id, lang::type langid)
{
	I18NStringStore *store = RAW(langid, id);
	panic_assert(!(store->info & INFO_ISFUNC), "attempt to get parameter-less function");
	return store->string;
}

const char *i18n::detail::getstr_param(str::type id, const std::vector<std::string>& params)
{
	I18NStringStore *store = RAW(get_nsettings()->lang, id);
	if(store->info & INFO_ISFUNC)
		return store->function(params);
	else
		return store->string;
}

// https://www.3dbrew.org/wiki/Country_Code_List
//  only took over those that we actually use
namespace CountryCode
{
	enum _codes {
		canada         =  18,
		greece         =  79,
		hungary        =  80,
		latvia         =  84,
		poland         =  97,
		romania        =  99,
		spain          = 105,
		united_kingdom = 110,
	};
}

// Not documented so gotten through a test application
namespace ProvinceCode
{
	enum _codes {
		uk_wales      =  5,
		sp_catalonia  = 11,
		japan_osaka   = 28,
		japan_okinawa = 48,
	};
}

#define DEFAULT_LANG(id) IS_STALLED_##id ? lang::english : lang::id

lang::type i18n::default_lang()
{
	u8 syslang = 0;
	u8 countryinfo[4];
	if(R_FAILED(CFGU_GetSystemLanguage(&syslang)))
		syslang = CFG_LANGUAGE_EN;
	/* countryinfo */
	if(R_FAILED(CFGU_GetConfigInfoBlk2(4, 0x000B0000, countryinfo)))
	{
		countryinfo[2] = 0;
		countryinfo[3] = 0;
	}

	switch(countryinfo[3])
	{
	case CountryCode::hungary: return DEFAULT_LANG(hungarian);
	case CountryCode::romania: return DEFAULT_LANG(romanian);
	case CountryCode::latvia: return DEFAULT_LANG(latvian);
	case CountryCode::poland: return DEFAULT_LANG(polish);
	case CountryCode::greece: return DEFAULT_LANG(greek);
	case CountryCode::spain:
#if !IS_STALLED_catalan /* if Catalan is stalled users would probably want normal Spanish */
		if(countryinfo[2] == ProvinceCode::sp_catalonia)
			return lang::catalan;
		break;
#endif
	case CountryCode::united_kingdom:
		if(countryinfo[2] == ProvinceCode::uk_wales)
			return DEFAULT_LANG(welsh); /* if Welsh would be stalled users would want English; the default for stalled langs */
		break;
	}

	switch(syslang)
	{
	case CFG_LANGUAGE_JP:
		switch(countryinfo[2])
		{
#if !IS_STALLED_ryukyuan
		case ProvinceCode::japan_okinawa: return lang::ryukyuan;
#endif
#if !IS_STALLED_jp_osaka
		case ProvinceCode::japan_osaka: return lang::jp_osaka;
#endif
		}
		return DEFAULT_LANG(japanese);
	case CFG_LANGUAGE_FR:
#if IS_STALLED_fr_canada
		return DEFAULT_LANG(french);
#else
		return countryinfo[3] == CountryCode::canada
			? lang::fr_canada : DEFAULT_LANG(french);
#endif
	case CFG_LANGUAGE_DE: return DEFAULT_LANG(german);
	case CFG_LANGUAGE_IT: return DEFAULT_LANG(italian);
	case CFG_LANGUAGE_ES: return DEFAULT_LANG(spanish);
	case CFG_LANGUAGE_ZH: return DEFAULT_LANG(schinese);
	case CFG_LANGUAGE_KO: return DEFAULT_LANG(korean);
	case CFG_LANGUAGE_NL: return DEFAULT_LANG(dutch);
	case CFG_LANGUAGE_PT: return DEFAULT_LANG(portuguese);
	case CFG_LANGUAGE_RU: return DEFAULT_LANG(russian);
	case CFG_LANGUAGE_TW: return DEFAULT_LANG(tchinese);
	case CFG_LANGUAGE_EN: // fallthrough
	default: return lang::english;
	}
}

