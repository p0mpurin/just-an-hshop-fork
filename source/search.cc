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

#include "search.hh"

#include <widgets/meta.hh>

#include <ui/menuselect.hh>
#include <ui/selector.hh>
#include <ui/checkbox.hh>
#include <ui/loading.hh>
#include <ui/swkbd.hh>
#include <ui/base.hh>

#include "installgui.hh"
#include "extmeta.hh"
#include "hsapi.hh"
#include "mng.hh"
#include "next.hh"
#include "util.hh"
#include "i18n.hh"

#include <climits>

static const char *short_keys[8] = { "i", "e", "sb", "sd", "t", "p", "pr", "u" };
#define SHORT_KEYS_SIZE (sizeof(short_keys) / sizeof(const char *))

static const std::unordered_map<std::string, std::string> keys =
{
	{ "include"     , "i"  },
	{ "inc"         , "i"  },
	{ "exclude"     , "e"  },
	{ "ex"          , "e"  },
	{ "direction"   , "sd" },
	{ "order"       , "sd" },
	{ "d"           , "sd" },
	{ "o"           , "sd" },
	{ "sort"        , "sb" },
	{ "s"           , "sb" },
	{ "r"           , "sb" },
	{ "titleid"     , "t"  },
	{ "tid"         , "t"  },
	{ "type"        , "p"  },
	{ "typ"         , "p"  },
	{ "productcode" , "pr" },
	{ "prodcode"    , "pr" },
	{ "product"     , "pr" },
	{ "pcode"       , "pr" },
	{ "prod"        , "pr" },
	{ "code"        , "pr" },
	{ "uniqueid"    , "u"  },
	{ "uid"         , "u"  },
	{ "uniq"        , "u"  },
};

static const std::unordered_map<std::string, std::string> sort_by =
{
	{ "size"        , "size"         },
	{ "blocks"      , "size"         },
	{ "name"        , "name"         },
	{ "category"    , "category"     },
	{ "type"        , "category"     },
	{ "subcategory" , "subcategory"  },
	{ "region"      , "subcategory"  },
	{ "added"       , "added_date"   },
	{ "uploaded"    , "added_date"   },
	{ "updated"     , "updated_date" },
	{ "edited"      , "updated_date" },
	{ "tid"         , "title_id"     },
	{ "id"          , "id"           },
	{ "downloads"   , "downloads"    },
	{ "dls"         , "downloads"    },
};

static const std::unordered_map<std::string, std::string> sort_direction =
{
	{ "ascending"  , "ascending"  },
	{ "asc"        , "ascending"  },
	{ "descending" , "descending" },
	{ "desc"       , "descending" },
};

using SearchParams = std::unordered_map<std::string, std::string>;

static bool is_hex(const std::string& str)
{
	for(size_t i = 0; i < str.size(); ++i)
		/* [A-Fa-f0-9] */
		if(!((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'f') || (str[i] >= 'A' && str[i] <= 'F')))
			return false;

	return true;
}

static bool is_tid(const std::string& str)
{
	return
		str.size() == 16 &&
		is_hex(str) &&
		str.find("0004") == 0;
}

static bool is_unique_id(const std::string& str)
{
	int sz = str.size();
	const char *rawstr = str.c_str();

	if (!sz || sz < 1 || sz > 7)
		return false;

	if (strspn(rawstr, "x0123456789abcdefABCDEF") != sz)
		return false;

	int n_x = 0;
	for (const char *i = rawstr; *i; i++)
		if (*i == 'x') n_x++;

	if (n_x != 0 && n_x != 1) /* either one or zero 'x's */
		return false;

	if (n_x == 1 && sz < 3) /* can't have a string composed of just 0x */
		return false;

	if (n_x == 1 && (rawstr[0] != '0' || rawstr[1] != 'x'))
		return false; /* can't have an 'x' when the 'x' is not part of ^0x */

	int parsed = strtoul(rawstr, NULL, 16);
	if (parsed < 0 || parsed == ULONG_MAX)
		return false;

	return parsed >= 0 && parsed <= 0xFFFFF;
}

static bool is_valid_query(const std::string& str)
{
	return str.size() >= 1;
}

static bool is_prod(const std::string& str)
{
	if(str.size() < 1 || str.size() > 16) return false;
	for(size_t i = 0; i < str.size(); ++i)
		/* [A-Za-z0-9-] */
		if(!((str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= 'a' && str[i] <= 'z') || (str[i] >= '0' && str[i] <= '9') || str[i] == '-'))
			return false;
	return true;
}

static bool is_hsid(const std::string& str)
{
	if(str.size() == 0) return false;
	for(size_t i = 0; i < str.size(); ++i)
		if(!(str[i] >= '0' && str[i] <= '9'))
			return false;
	uint64_t raw = strtoull(str.c_str(), NULL, 10);
	return raw > 0 && raw < UINT32_MAX;
}

static bool is_filter_name(const std::string& str, bool allow_wildcard)
{
	if(allow_wildcard && str == "*")
		return true;

	for(size_t i = 0; i < str.size(); ++i)
  		if(!((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z') || str[i] == '-'))
    		return false;

	return !str.empty();
}

static void str_split(std::vector<std::string>& ret, const std::string& s, const std::string& p)
{
	size_t find_idx = s.find(p);
	size_t cur_idx = 0;
	if(find_idx == std::string::npos)
	{
		ret.push_back(s);
		return;
	}

	do
	{
		std::string part = s.substr(cur_idx, find_idx - cur_idx);
		if(!part.empty() && part != p) ret.push_back(part);
		cur_idx = find_idx + p.size();
	} while((find_idx = s.find(p, cur_idx)) != std::string::npos);

	if(!s.empty())
		ret.push_back(s.substr(cur_idx));
}

static bool has_key(std::unordered_map<std::string, std::string> map, const std::string& key)
{
	return map.find(key) != map.end();
}

static bool is_short_key(const std::string& key)
{
	for(size_t i = 0; i < SHORT_KEYS_SIZE; ++i)
		if(strcmp(key.c_str(), short_keys[i]) == 0)
			return true;

	return false;
}

static bool contains(const std::vector<std::string>& list, const std::string& s)
{
	for(const std::string& elem : list)
		if(elem == s) return true;

	return false;
}

static bool parse_filters(std::string& ret, std::vector<std::string>& out_categories, const std::string& raw_filters)
{
	std::vector<std::string> out_filters;
	std::vector<std::string> filter_list_raw;

	std::string rf = raw_filters;
	trim(rf, ",.");
	str_split(filter_list_raw, rf, ",");

	if(filter_list_raw.size() == 0 || filter_list_raw.size() > 15)
		return false;

	for(const std::string& filter : filter_list_raw)
	{
		std::vector<std::string> filter_split;
		str_split(filter_split, filter, ".");

		switch(filter_split.size())
		{
		case 0:
			return false;
		case 1:
		{
			if(!is_filter_name(filter_split[0], false))
				return false;
			std::string cur_filter = filter_split[0] + ".*";
			if(contains(out_filters, cur_filter))
				return false;
			out_categories.push_back(filter_split[0]);
			out_filters.push_back(cur_filter);
			break;
		}
		case 2:
		{
			if(!is_filter_name(filter_split[0], false) || !is_filter_name(filter_split[1], true))
				return false;
			std::string cur_filter = filter_split[0] + "." + filter_split[1];
			if(contains(out_filters, cur_filter))
				return false;
			out_categories.push_back(filter_split[0]);
			out_filters.push_back(cur_filter);
			break;
		}
		default:
			return false;
		}
	}

	join(ret, out_filters, ",");
	return true;
}

static void error(str::type strid)
{
	ui::I18NEnabledRenderQueue queue;

	ui::builder<ui::Text>(ui::Screen::top, strid)
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.wrap()
		.add_to(queue);

	queue.render_finite_button(KEY_A | KEY_B | KEY_START);
}

/* Returns NULL if success or an error string if the input couldn't be parsed */
str::type parse_legacy_search_query(const std::string& input, SearchParams& out_params)
{
	std::unordered_map<std::string, std::string> params;
	std::vector<std::string> parts;
	std::string query = "";

	str_split(parts, input, " ");

	for(const std::string& part : parts)
	{
		std::vector<std::string> param_pair;
		str_split(param_pair, part, ":");
		bool short_key = is_short_key(param_pair[0]);
		if(param_pair.size() != 2 || (!short_key && !has_key(keys, param_pair[0])))
		{
			query += part + " ";
			continue;
		}
		out_params[short_key ? param_pair[0] : keys.at(param_pair[0])] = param_pair[1];
	}

	trim(query, " ");

	if(!query.empty())
	{
		out_params["q"] = query;
		out_params["qt"] = "Text";
	}

	bool has_tid = has_key(out_params, "t");
	bool has_prod = has_key(out_params, "pr");
	bool has_uniq = has_key(out_params, "u");

	if(has_tid)
	{
		if(out_params.size() > 1)    return str::no_other_params_tid;
		if(!is_tid(out_params["t"])) return str::invalid_tid;
		out_params["q"] = out_params["t"];
		out_params["qt"] = "TitleID";
		out_params.erase("t");
	}

	if(has_prod)
	{
		if(out_params.size() > 1)      return str::no_other_params_prod;
		if(!is_prod(out_params["pr"])) return str::invalid_prod;

		out_params["q"] = out_params["pr"];
		out_params["qt"] = "ProductCode";
		out_params.erase("pr");
	}

	if(has_uniq)
	{
		if(out_params.size() > 1)          return str::no_other_params_uniq;
		if(!is_unique_id(out_params["u"])) return str::invalid_uniq;

		out_params["q"] = out_params["u"];
		out_params["qt"] = "UniqueID";
		out_params.erase("u");
	}

	/* This error message isn't entirely correct but good enough */
	if(!has_tid && !has_prod && !has_uniq && query.size() < 1)
		return str::query_too_short;

	bool has_sb = has_key(out_params, "sb");
	bool has_sd = has_key(out_params, "sd");
	bool has_e = has_key(out_params, "e");
	bool has_i = has_key(out_params, "i");

	if((has_sb && !has_sd) || (has_sd && !has_sb))
		return str::both_sd_and_sb;

	if(has_sb)
	{
		if(!has_key(sort_by, out_params["sb"]))
			return str::invalid_sb;

		out_params["sb"] = sort_by.at(out_params["sb"]);
	}

	if(has_sd)
	{
		if(!has_key(sort_direction, out_params["sd"]))
			return str::invalid_sd;

		out_params["sd"] = sort_direction.at(out_params["sd"]);
	}

	if(has_key(out_params, "p") && (out_params["p"] != "legit" && out_params["p"] != "piratelegit" && out_params["p"] != "standard"))
		return str::invalid_content_type;

	std::string includes, excludes;
	std::vector<std::string> include_cats, exclude_cats;

	if(has_i && !parse_filters(includes, include_cats, out_params["i"])) return str::invalid_includes;
	if(has_e && !parse_filters(excludes, exclude_cats, out_params["e"])) return str::invalid_excludes;

	if(!includes.empty()) out_params["i"] = includes;
	if(!excludes.empty()) out_params["e"] = excludes;

	for(const std::string& include_cat : include_cats)
		for(const std::string& exclude_cat : exclude_cats)
			if(include_cat == exclude_cat)
				return str::filter_overlap;

	return str::_i_max;
}

enum class ContentType {
	Legit,
	PirateLegit,
	Standard,
	All,
};

static const char *content_type_tab[] = { "legit", "piratelegit", "standard" };

template <typename T>
static void vec_erase_if(std::vector<T>& vec, std::function<bool(const T&)> func)
{
	for(size_t i = 0; i < vec.size(); ++i)
		if(func(vec[i]))
		{
			vec.erase(vec.begin() + i);
			--i; /* vec now has one less element, we have to adjust i to account for this */
		}
}

#define MAX_TYPE_FILTER 4 /* 4 multiline filters will fuck up the rest of the page just a little, so that'll be the max */
#define FILTER_INCLUDE  0
#define FILTER_EXCLUDE  1

UI_CTHEME_GETTER(color_warning, ui::theme::warning_color)
UI_STATIC_SLOTS(WarningColorSlot, "__nsearch_warning_color", color_warning)

UI_CTHEME_GETTER(color_x, ui::theme::x_color)
UI_STATIC_SLOTS(XColorSlot, "__nsearch_x_color", color_x)

static std::string extract_category_from_filter(const std::string& filter)
{
	const char *str = filter.c_str();
	const char *dot = strchr(str, '.');
	return std::string(str, dot - str);
}

static std::string extract_subcategory_from_filter(const std::string& filter)
{
	return std::string(strchr(filter.c_str(), '.') + 1);
}

using filterTextsType = ui::Text *(*)[MAX_TYPE_FILTER + 1];

static bool should_skip_due_to_filters(filterTextsType filterTexts, int filterCount[], const std::string& category, const std::string& subcategory)
{
	/* we cannot include anything more if we already include everything from a category
	 * and we cannot include anything that has already been included in the other; we must
	 * walk through both types filters and remove anything that has a specific subcategory */
	for(int i = 0; i < 2; ++i)
		for(int j = 0; j < filterCount[i]; ++j)
		{
			const std::string& filter = filterTexts[i][j + 1]->get_text();
			if(extract_category_from_filter(filter) == category && extract_subcategory_from_filter(filter) == subcategory)
				return true;
		}
	return false;
}

/* returns true if a filter was not chosen */
static bool show_filter_subcat_select(std::string& ret, filterTextsType filterTexts, int filterCount[], hsapi::Category *category)
{
	ui::MenuSelect *msel;
	ui::I18NEnabledRenderQueue rq;

	std::vector<hsapi::Subcategory *> subcategories;
	subcategories.reserve(category->subcategories.size());

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.when_select([&ret, &msel, &category, &subcategories]() -> bool {
			hsapi::Subcategory *scat = subcategories[msel->pos()];
			ret = category->name + "." + (scat ? scat->name : "*");
			return false;
		})
		.add_to(&msel, rq);

	for(int i = 0; i < 2; ++i)
		for(int j = 0; j < filterCount[i]; ++j)
			if(extract_category_from_filter(filterTexts[i][j + 1]->get_text()) == category->name)
				goto no_add_star;
	subcategories.push_back(nullptr);
	msel->add_row(str::everything);
no_add_star:

	for(auto it = category->subcategories.begin(); it != category->subcategories.end(); ++it)
		if(!should_skip_due_to_filters(filterTexts, filterCount, category->name, it->second.name))
		{
			subcategories.push_back(&it->second);
			msel->add_row(it->second.disp);
		}

	rq.render_finite_button(KEY_B | KEY_START);
	return !ret.size();
}

/* returns true if a filter was chosen */
static bool show_filter_select(std::string& ret, filterTextsType filterTexts, int filterCount[])
{
	ui::MenuSelect *msel;
	ui::I18NEnabledRenderQueue rq;

	ret = "";

	std::vector<hsapi::Category *> categories;
	categories.reserve(hsapi::categories().size());

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.when_select([&ret, &filterTexts, &filterCount, &msel, &categories]() -> bool {
			return show_filter_subcat_select(ret, filterTexts, filterCount, categories[msel->pos()]);
		})
		.add_to(&msel, rq);

	for(auto it = hsapi::categories().begin(); it != hsapi::categories().end(); ++it)
		if(!should_skip_due_to_filters(filterTexts, filterCount, it->second.name, "*"))
		{
			categories.push_back(&it->second);
			msel->add_row(it->second.disp);
		}

	rq.render_finite_button(KEY_B | KEY_START);
	return !!ret.size();
}

static bool not_found_check(Result failure)
{
	if (failure == APPERR_API_FAIL && hsapi::last_error().reason() == hsapi::ErrorReason::NotFound) {
		error(str::search_zero_results);
		return true;
	}

	return false;
}

#define TAB_QUICK    0
#define TAB_FILTERED 1
#define TAB_LGY      2
#define TAB_TID      3
#define TAB_PROD     4
#define TAB_HSID     5
#define TAB_UNIQUEID 6

using kbd_checker_func = bool (*)(const std::string&);

struct search_tab {
	str::type heading;
	kbd_checker_func checker_func;
	ui::BaseWidget *bottomTab;
	ui::WidgetGroup group;
};

void show_search()
{
	/* hide the bottom bar and disable konami */
	bool focus = ui::set_focus(true);

	/* search may be the most complicated ui code in the whole 3hs project */

	ui::CheckBox *inc_unofficial, *inc_games, *inc_updates, *inc_dlc, *inc_vc, *inc_dsi;
	ui::CheckBox *reg_eur, *reg_usa, *reg_jpn, *reg_other;
	ui::Text *cheading, *modehint, *widest_include_content;
	ContentType ctype = ContentType::All;
	ui::KBDEnabledButton *kbd;

	size_t tabIndex = 0;

	ui::Button *submit;
	ui::I18NEnabledRenderQueue rq;

	constexpr float filter_padding = 5.0f;
	constexpr float filter_div = (ui::screen_width(ui::Screen::bottom) - filter_padding * 2.0f) / 2.0f;

	static constexpr int ntabs = 7;

	search_tab search_tabs[ntabs] = {
		{ str::quick,             is_valid_query },
		{ str::filters,           is_valid_query },
		{ str::legacy_search_tab, nullptr        },
		{ str::tid,               is_tid         },
		{ str::prodcode,          is_prod        },
		{ str::landing_id,        is_hsid        },
		{ str::unique_id,         is_unique_id   },
	};
	ui::WidgetGroup ctypeSelGrp;
	ui::WidgetGroup filterGroups[2][MAX_TYPE_FILTER];
	ui::Text *filterTexts[2][MAX_TYPE_FILTER + 1];
	int filterCount[2] = { 0, 0 };
	ui::Button *addButtons[2];
	ui::Text *invalidInputTop;
	ui::Text *invalidInput;

	SearchParams legacy_params;

	auto perform_check = [&kbd, &submit, &invalidInput, &invalidInputTop, &legacy_params, &search_tabs](size_t idx) -> void {
		str::type err_msg = str::_i_max;
		ui::Text *selectedInvalidInput = invalidInput;
		bool check_ok = false;

		if(idx == TAB_LGY)
		{
			/* The check actually parses the error... */
			legacy_params.clear();
			err_msg = parse_legacy_search_query(kbd->value(), legacy_params);
			check_ok = err_msg == str::_i_max;
		}
		else
		{
			check_ok = search_tabs[idx].checker_func(kbd->value());
			if(idx == TAB_TID && !check_ok)
				err_msg = str::new_invalid_tid;
			else if (idx == TAB_HSID && !check_ok)
				err_msg = str::invalid_hsid;
			else if (idx == TAB_UNIQUEID && !check_ok)
				err_msg = str::invalid_uniq;
			else if(idx == TAB_PROD && !check_ok)
				err_msg = str::new_invalid_prod;
			else if ((idx == TAB_QUICK || idx == TAB_FILTERED) && !check_ok) {
				err_msg = str::query_too_short;
				selectedInvalidInput = invalidInputTop;
			}
		}

		submit->set_hidden(!check_ok);

		/* If we want to display an error text we can */
		if(!kbd->is_empty() && err_msg != str::_i_max)
		{
			selectedInvalidInput->set_text(err_msg);
			selectedInvalidInput->set_x(ui::layout::center_x);
			selectedInvalidInput->set_y(ui::layout::center_y);
			invalidInput->set_hidden(selectedInvalidInput != invalidInput);
			invalidInputTop->set_hidden(selectedInvalidInput == invalidInput);
		}
		else {
			invalidInput->set_hidden(true);
			invalidInputTop->set_hidden(true);
		}
	};

	ui::builder<ui::Text>(ui::Screen::bottom)
		.x(ui::layout::center_x).y(ui::layout::center_y)
		.hide().wrap()
		.swap_slots(WarningColorSlot)
		.add_to(&invalidInput, rq);

	ui::builder<ui::Text>(ui::Screen::top)
		.x(ui::layout::center_x).y(ui::layout::bottom)
		.hide().wrap()
		.swap_slots(WarningColorSlot)
		.add_to(&invalidInputTop, rq);

	ui::builder<ui::ButtonCallback>(ui::Screen::none, KEY_L | KEY_R)
		.when_kdown([&](u32 keys) -> bool {
			size_t newIndex;
			if(keys & KEY_R) newIndex = tabIndex == ntabs - 1 ? 0 : tabIndex + 1;
			else if(keys & KEY_L) newIndex = tabIndex == 0 ? ntabs - 1 : tabIndex - 1;
			else return true; /* ? should never happen */
			search_tabs[tabIndex].group.set_hidden(true);
			search_tabs[newIndex].group.set_hidden(false);
			cheading->set_text(search_tabs[newIndex].heading);
			modehint->set_x(ui::right(cheading, modehint, 4.0f));
			if(search_tabs[newIndex].bottomTab)
			{
				ctypeSelGrp.position_under(search_tabs[newIndex].bottomTab);
				ctypeSelGrp.set_hidden(false);
			}
			else ctypeSelGrp.set_hidden(true);
			/* slightly cursed, we need to hide some filterGroup elements */
			if(newIndex == TAB_FILTERED)
				for(int i = 0; i < 2; ++i)
				{
					for(int j = filterCount[i]; j < MAX_TYPE_FILTER; ++j)
						filterGroups[i][j].set_hidden(true);
					if(filterCount[i] == MAX_TYPE_FILTER)
						addButtons[i]->set_hidden(true);
				}
			kbd->set_numpad_mode(newIndex == TAB_HSID);
			/* the input may be invalid for the new mode... */
			perform_check(newIndex);
			tabIndex = newIndex;
			return true;
		})
		.add_to(rq);

	ui::builder<ui::Text>(ui::Screen::bottom, search_tabs[tabIndex].heading)
		.size(0.55f)
		.x(ui::layout::left)
		.y(6.0f)
		.add_to(&cheading, rq);

	ui::builder<ui::Text>(ui::Screen::bottom, str::press_to_switch)
		.size(0.45f)
		.right(rq.back(), 5.0f)
		.align_y(rq.back())
		.add_to(&modehint, rq);

	ui::builder<ui::KBDEnabledButton>(ui::Screen::bottom, str::empty, str::enter_query)
		.x(ui::layout::center_x)
		.under(rq.back(), 5.0f)
		.when_update([&tabIndex, &perform_check](ui::KBDEnabledButton *) -> void {
			/* set the submit button hidden for invalid input/reparse input (legacy) */
			perform_check(tabIndex);
		})
		.add_to(&kbd, rq);

	ui::builder<ui::Button>(ui::Screen::bottom, str::submit)
		.wrap()
		.x(ui::layout::right)
		.y(ui::layout::bottom)
		.hide() /* should be hidden by default since there'll be no input yet */
		.when_clicked([&](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then([&]() -> void {
				std::unordered_map<std::string, std::string> params;
				std::vector<hsapi::PartialTitle> titles;

				switch (tabIndex)
				{
					case TAB_HSID:
						{
							hsapi::Title title;
							Result res = hsapi::call<hsapi::Title &>(
								[](Result r) -> bool {
									if (r == APPERR_API_FAIL && hsapi::last_error().reason() == hsapi::ErrorReason::NotFound) {
										error(str::search_zero_results);
										return true;
									}
									return false;
								},
								hsapi::get_by_id,
								title,
								std::move((hsapi::hid)kbd->numpad_value())
							);
							if (R_SUCCEEDED(res) && show_extmeta(title))
								install::gui::hs_cia(title);

							return;
						}
						break;
					case TAB_LGY:
						/* It is a bit of a waste to copy, but whatever */
						params = legacy_params;
						break;
					default:
						{
							if((tabIndex == TAB_QUICK || tabIndex == TAB_FILTERED) && ctype != ContentType::All)
								params["p"] = content_type_tab[(int) ctype];

							switch (tabIndex)
							{
							case TAB_QUICK:
							case TAB_FILTERED:
								params["qt"] = "Text";
								break;
							case TAB_TID:
								params["qt"] = "TitleID";
								break;
							case TAB_PROD:
								params["qt"] = "ProductCode";
								break;
							case TAB_UNIQUEID:
								params["qt"] = "UniqueID";
							}

							params["q"] = kbd->value();

							/* both quick(0) and filtered(1) have additional parameters that we need to set */
							if(tabIndex == TAB_QUICK)
							{
								std::string p;
								bool exclude = false;
								const char *key = NULL;
								auto do_checkbox = [&](bool is_checked, const std::string& cat) -> void {
									if((!is_checked && !exclude) || (is_checked && exclude))
										return;
									if(p.size()) p += ",";
									/* everything after here should probably be modified sometime in the future */
									p += cat + ".*";
								};

								if(!inc_unofficial->checked()) /* include mode */
									key = "i";
								/* if all are checked do nothing; we want everything, exclude mode */
								else if(!(/* inc_unofficial->checked() && */ inc_games->checked() && inc_updates->checked() && inc_dlc->checked()))
								{
									key = "e";
									exclude = true;
								}

								if(key)
								{
									do_checkbox(inc_games->checked(), "games");
									do_checkbox(inc_updates->checked(), "updates");
									do_checkbox(inc_dlc->checked(), "dlc");
									do_checkbox(inc_vc->checked(), "virtual-console");
									do_checkbox(inc_dsi->checked(), "dsiware");
									params[key] = p;
								}
							}
							else if(tabIndex == TAB_FILTERED)
							{
								auto make_list = [](std::string& str, ui::Text *texts[], int count) -> void {
									str = texts[0]->get_text();
									for(int i = 1; i < count; ++i)
										str += "," + texts[i]->get_text();
								};

								/* assume all invalid input is already blocked at an earlier stage */
								if(filterCount[FILTER_INCLUDE]) make_list(params["i"], &filterTexts[FILTER_INCLUDE][1], filterCount[FILTER_INCLUDE]);
								if(filterCount[FILTER_EXCLUDE]) make_list(params["e"], &filterTexts[FILTER_EXCLUDE][1], filterCount[FILTER_EXCLUDE]);
							}
						}
				}

				Result res = hsapi::call<std::vector<hsapi::PartialTitle>&, const std::unordered_map<std::string, std::string>&>(hsapi::search, titles, params);
				if(R_SUCCEEDED(res))
				{
					/* we may need to post-process the regions */
					if(tabIndex == TAB_QUICK)
					{
						if(!reg_other->checked()) /* include mode */
							vec_erase_if<hsapi::PartialTitle>(titles, [&](const hsapi::PartialTitle& title) -> bool {
								hsapi::Subcategory& subcat = hsapi::subcategory(title.cat, title.subcat);
								return (reg_usa->checked() && subcat.name != REGION_USA) || (reg_eur->checked() && subcat.name != REGION_EUROPE) || (reg_jpn->checked() && subcat.name != REGION_JAPAN);
							});
						else if(!(/* reg_other->checked() && */ reg_usa->checked() && reg_eur->checked() && reg_jpn->checked())) /* exclude mode */
							vec_erase_if<hsapi::PartialTitle>(titles, [&](const hsapi::PartialTitle& title) -> bool {
								hsapi::Subcategory& subcat = hsapi::subcategory(title.cat, title.subcat);
								return (!reg_usa->checked() && subcat.name == REGION_USA) || (!reg_eur->checked() && subcat.name == REGION_EUROPE) || (!reg_jpn->checked() && subcat.name == REGION_JAPAN);
							});
					}
					if(titles.size())
						next::maybe_install_title(titles);
					else
						error(str::search_zero_results);
				}
			});
			return true;
		})
		.add_to(&submit, rq);

	/* global filtering for content type */
	ui::builder<ui::Text>(ui::Screen::bottom, str::content_type)
		.x(ui::layout::left)
		.above(rq.back())
		.add_to(rq);
	rq.group_last(ctypeSelGrp);

	std::vector<std::string> ctypeLabels = { STRING(all), "Legit", "Pirate Legit", "Standard" };
	static const std::vector<ContentType> ctypeValues = { ContentType::All, ContentType::Legit, ContentType::PirateLegit, ContentType::Standard };
	ui::builder<ui::Selector<ContentType>>(ui::Screen::bottom, ctypeLabels, ctypeValues, &ctype)
		.manual_i18n_update([&ctypeLabels](ui::Selector<ContentType> *s, lang::type nlang) -> void {
			ctypeLabels[0] = i18n::getstr(str::all, nlang);
			s->set_labels(ctypeLabels);
		})
		.size_children(0.50f)
		.wrap()
		.x(ui::layout::left)
		.seek_to(ctype)
		.exclusive_mode(false)
		.add_to(rq);
	rq.group_last(ctypeSelGrp);

	/* setup for the quick tab */
	ui::builder<ui::Text>(ui::Screen::bottom, str::include_content)
		.size(0.6f).x(filter_padding).under(kbd)
		.max_width(filter_div)
		.add_to(&widest_include_content, rq);
	rq.group_last(search_tabs[TAB_QUICK].group);

#define OPT(label_content, output_var, ...)  \
		ui::builder<ui::CheckBox>(ui::Screen::bottom, true) \
			. __VA_ARGS__ \
			.under(rq.back()) \
			.add_to(output_var, rq); \
		rq.group_last(search_tabs[0].group); \
		ui::builder<ui::Text>(ui::Screen::bottom, label_content) \
			.right(rq.back()).align_y(rq.back()).middle(rq.back()) \
			.size(0.5f) \
			.add_to(rq); \
		rq.group_last(search_tabs[TAB_QUICK].group);

		OPT("Games", &inc_games, align_x(rq.back(), 8.0f))
		OPT("Updates", &inc_updates, align_x(inc_games))
		OPT("DLC", &inc_dlc, align_x(inc_games))
		OPT("Virtual Console", &inc_vc, align_x(inc_games))
		OPT("DSiWare", &inc_dsi, align_x(inc_games))
		OPT(str::other, &inc_unofficial, align_x(inc_games))

		search_tabs[TAB_QUICK].bottomTab = rq.back();

		ui::builder<ui::Text>(ui::Screen::bottom, str::regions)
			.size(0.6f).x(filter_padding + filter_div).under(kbd)
			.max_width(filter_div)
			.add_to(rq);
		rq.group_last(search_tabs[TAB_QUICK].group);

		OPT("North America", &reg_usa, align_x(rq.back(), 8.0f))
		OPT("Europe", &reg_eur, align_x(reg_usa))
		OPT("Japan", &reg_jpn, align_x(reg_usa))
		OPT(str::other, &reg_other, align_x(reg_usa))

		switch(ctr::mng::get_system_region())
		{
		case CFG_REGION_EUR:
			reg_usa->set_checked(false);
			reg_jpn->set_checked(false);
			break;
		case CFG_REGION_JPN:
			reg_usa->set_checked(false);
			reg_eur->set_checked(false);
			break;
		case CFG_REGION_USA:
			reg_eur->set_checked(false);
			reg_jpn->set_checked(false);
			break;
		default:
			/* on the less common regions we just do nothing */
			break;
		}

#undef OPT
	/* setup for the filters tab */
		str::type filterLabels[] = { str::include_filters, str::exclude_filters };
		for(int i = 0; i < 2; ++i)
		{
			ui::builder<ui::Text>(ui::Screen::bottom, filterLabels[i])
				.size(0.60f).x(filter_padding + filter_div*i)
				.max_width(filter_div)
				.under(kbd)
				.add_to(&filterTexts[i][0], rq);
			rq.group_last(search_tabs[TAB_FILTERED].group);

			ui::builder<ui::Button>(ui::Screen::bottom, str::add)
				.align_x(rq.back()).under(rq.back()).wrap()
				.when_clicked([i, &filterTexts, &filterGroups, &filterCount, &addButtons, &search_tabs, &ctypeSelGrp](ui::Button *) -> bool {
					ui::RenderQueue::global()->render_and_then([i, &filterTexts, &filterGroups, &filterCount, &addButtons, &search_tabs, &ctypeSelGrp]() -> void {
						std::string filter;
						if(!show_filter_select(filter, filterTexts, filterCount))
							return; /* nothing to do */

						ui::Text *text = filterTexts[i][1 + filterCount[i]];
						ui::WidgetGroup& group = filterGroups[i][filterCount[i]];
						text->set_text(filter);
						group.set_hidden(false);
						group.position_under_horizontal(filterCount[i] ? filterGroups[i][filterCount[i] - 1].lowest() : filterTexts[i][0]);
						ui::BaseWidget *lowest = group.lowest();
						addButtons[i]->set_y(ui::under(lowest, addButtons[i]));

						++filterCount[i];
						/* can't add more filters */
						if(filterCount[i] == MAX_TYPE_FILTER)
						{
							addButtons[i]->set_hidden(true);
							/* and now the bottom isn't add_include anymore */
							if(i == FILTER_INCLUDE)
								search_tabs[TAB_FILTERED].bottomTab = lowest;
						}
						/* we need to reposition ctypeSelGrp too here */
						if(i == FILTER_INCLUDE)
							ctypeSelGrp.position_under(search_tabs[TAB_FILTERED].bottomTab);
					});
					return true;
				})
				.add_to(&addButtons[i], rq);
			rq.group_last(search_tabs[TAB_FILTERED].group);
		}

		for(int i = 0; i < 2; ++i)
			for(int j = 0; j < MAX_TYPE_FILTER; ++j)
			{
				/* TODO: This should be a sprite probably */
				ui::builder<ui::Button>(ui::Screen::bottom, "X")
					.size_children(0.45f) /* temp */
					.wrap()
					.swap_slots(XColorSlot)
					.align_x(filterTexts[i][0], 4.0f)
					/* y pos is set later */
					.disable_background()
					.when_clicked([i, j, &filterCount, &filterTexts, &filterGroups, &addButtons, &search_tabs, &ctypeSelGrp](ui::Button *) -> bool {
						/* we must move up all other filters under us and then delete the final one */
						panic_assert(filterCount[i], "button pressed when it should be hidden");
						for(int k = j, l = filterCount[i] - 1; k < l; ++k)
							filterTexts[i][k + 1]->set_text(filterTexts[i][k + 2]->get_text());
						--filterCount[i];
						/* just for good measures let's just reposition all texts (for multiline filters) */
						for(int k = 0; k < filterCount[i]; ++k)
							filterGroups[i][k].position_under_horizontal(k ? filterGroups[i][k - 1].lowest() : filterTexts[i][0]);
						filterGroups[i][filterCount[i]].set_hidden(true);
						addButtons[i]->set_hidden(false);
						addButtons[i]->set_y(ui::under(filterCount[i] ? filterGroups[i][filterCount[i] - 1].lowest() : filterTexts[i][0], addButtons[i]));
						/* we may need to repositition some stuff */
						if(i == FILTER_INCLUDE)
						{
							search_tabs[TAB_FILTERED].bottomTab = addButtons[i];
							ctypeSelGrp.position_under(search_tabs[TAB_FILTERED].bottomTab);
						}
						return true;
					})
					.hide()
					.add_to(rq);
				rq.group_last(search_tabs[TAB_FILTERED].group);
				rq.group_last(filterGroups[i][j]);
				ui::builder<ui::Text>(ui::Screen::bottom)
					.size(0.45f)
					.right(rq.back())
					/* y pos is set later */
					.max_width(filter_div - 4.0f*3.0f /* padding */)
					.wrap().hide()
					.add_to(&filterTexts[i][j + 1], rq);
				rq.group_last(search_tabs[TAB_FILTERED].group);
				rq.group_last(filterGroups[i][j]);
			}

		/* by default there are no filters: the add button will be the bottom widget */
		search_tabs[TAB_FILTERED].bottomTab = addButtons[FILTER_INCLUDE];

	/* the others don't require a content type filter, so it'll be disabled */
	search_tabs[TAB_LGY].bottomTab  = search_tabs[TAB_TID].bottomTab  = nullptr;
	search_tabs[TAB_PROD].bottomTab = search_tabs[TAB_HSID].bottomTab = nullptr;

	ctypeSelGrp.position_under(search_tabs[TAB_QUICK].bottomTab);
	for(int i = 1; i < ntabs; ++i)
		search_tabs[i].group.set_hidden(true);

	ui::builder<ui::ButtonCallback>(ui::Screen::none, KEY_A)
		.when_kdown([submit, &kbd](u32) -> bool {
			if(!submit->is_hidden())
				submit->press();
			else
				kbd->press();

			return true;
		})
		.add_to(rq);

	rq.render_finite_button(KEY_B | KEY_START);
	ui::set_focus(false);
}

