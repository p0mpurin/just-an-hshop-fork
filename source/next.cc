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

#include "next.hh"

//!!!DELETE
#include <log.hh>

#include <widgets/meta.hh>
#include <ui/icongrid.hh>
#include <ui/list.hh>
#include <ui/base.hh>

#include <algorithm>
#include <ctype.h>

#include <3rd/stb_image.h>

#include "installgui.hh"
#include "extmeta.hh"
#include "hsapi.hh"
#include "mng.hh"
#include "queue.hh"
#include "panic.hh"


hsapi::hcid next::sel_cat(size_t *cursor)
{
	panic_assert(hsapi::categories().size() > *cursor, "invalid cursor position");
	using list_t = ui::List<hsapi::Category>;

	ui::prev_desc desc = ui::set_desc(str::select_cat);
	bool focus = ui::set_focus(false);
	hsapi::hcid ret = next_cat_exit;

	ui::I18NEnabledRenderQueue queue;

	ui::CatMeta *meta;
	list_t *list;

	static std::vector<hsapi::Category> sorted_categories;
	if(sorted_categories.size() == 0)
		hsapi::sorted_categories(sorted_categories);
	const float category_list_height =
		std::min<size_t>(sorted_categories.size(), 12) * list_t::text_spacing;
	const float category_list_y =
		(ui::dimensions::height - category_list_height) / 2.0f;

	ui::builder<ui::CatMeta>(ui::Screen::bottom, sorted_categories[*cursor])
		.add_to(&meta, queue);

	ui::builder<list_t>(ui::Screen::top, &sorted_categories)
		.to_string([](const hsapi::Category& cat) -> std::string { return cat.disp; })
		.when_select([&ret](list_t *self, size_t i, u32 kDown) -> bool {
			ret = self->at(i).id;
			if(kDown & KEY_START)
				ret = next_cat_exit;
			return false;
		})
		.when_change([meta](list_t *self, size_t i) -> void {
			meta->set_cat(self->at(i));
		})
		.buttons(KEY_START)
		.x(5.0f).y(category_list_y)
		.add_to(&list, queue);

	if(cursor != nullptr) list->set_pos(*cursor);
	queue.render_finite();
	if(cursor != nullptr) *cursor = list->get_pos();

	ui::set_focus(focus);
	ui::set_desc(desc);
	return ret;
}

hsapi::hcid next::sel_sub(hsapi::hcid cat_id, size_t *cursor)
{
	using list_t = ui::List<hsapi::Subcategory>;

	hsapi::Category& cat = hsapi::category(cat_id);

	ui::prev_desc desc = ui::set_desc(str::select_subcat);
	bool focus = ui::set_focus(false);
	hsapi::hcid ret = next_sub_back;

	ui::I18NEnabledRenderQueue queue;

	ui::SubMeta *meta;
	list_t *list;

	panic_assert(cat.subcategories.size() > *cursor, "invalid cursor position");

	std::vector<hsapi::Subcategory> subcats;
	for(auto it = cat.subcategories.begin(); it != cat.subcategories.end(); ++it)
		subcats.push_back(it->second);

	ui::builder<ui::SubMeta>(ui::Screen::bottom, subcats[*cursor])
		.add_to(&meta, queue);

	ui::builder<list_t>(ui::Screen::top, &subcats)
		.to_string([](const hsapi::Subcategory& scat) -> std::string { return scat.disp; })
		.when_select([&ret](list_t *self, size_t i, u32 kDown) -> bool {
			ret = self->at(i).id;
			if(kDown & KEY_B) ret = next_sub_back;
			if(kDown & KEY_START) ret = next_sub_exit;
			return false;
		})
		.when_change([meta](list_t *self, size_t i) -> void {
			meta->set_sub(self->at(i));
		})
		.buttons(KEY_B | KEY_START)
		.x(5.0f).y(25.0f)
		.add_to(&list, queue);

	if(cursor != nullptr)
		list->set_pos(*cursor);

	if(ISET_GOTO_REGION)
	{
		const char *scname;
		switch(ctr::mng::get_system_region())
		{
		case CFG_REGION_AUS: scname = "australia"; break;
		case CFG_REGION_EUR: scname = "europe"; break;
		case CFG_REGION_CHN: scname = "china"; break;
		case CFG_REGION_JPN: scname = "japan"; break;
		case CFG_REGION_KOR: scname = "korea"; break;
		case CFG_REGION_TWN: scname = "taiwan"; break;
		case CFG_REGION_USA: scname = "north-america"; break;
		default: scname = NULL; break;
		}
		if(scname)
		{
			auto cat = std::find_if(subcats.begin(), subcats.end(), [scname](const hsapi::Subcategory& sc) -> bool { return sc.name == scname; });
			if(cat != subcats.end())
			{
				list->set_pos(std::distance(subcats.begin(), cat));
				meta->set_sub(*cat);
			}
		}
	}
	queue.render_finite();
	if(cursor != nullptr) *cursor = list->get_pos();

	ui::set_focus(focus);
	ui::set_desc(desc);
	return ret;
}

template <typename T>
using sort_callback = bool (*) (T& a, T& b);

static bool string_case_cmp(const std::string& a, const std::string& b, bool lt)
{
	for(size_t i = 0; i < a.size() && i < b.size(); ++i)
	{
		char cha = tolower(a[i]), chb = tolower(b[i]);
		if(cha == chb) continue;
		return lt ? cha < chb : cha > chb;
	}
	return lt ? a.size() < b.size() : a.size() > b.size();
}

static bool sort_alpha_desc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return string_case_cmp(a.name, b.name, false); }
static bool sort_tid_desc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.tid > b.tid; }
static bool sort_size_desc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.size > b.size; }
static bool sort_downloads_desc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.dlCount > b.dlCount; }
static bool sort_id_desc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.id > b.id; }

static bool sort_alpha_asc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return string_case_cmp(a.name, b.name, true); }
static bool sort_tid_asc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.tid < b.tid; }
static bool sort_size_asc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.size < b.size; }
static bool sort_downloads_asc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.dlCount < b.dlCount; }
static bool sort_id_asc(hsapi::PartialTitle& a, hsapi::PartialTitle& b) { return a.id < b.id; }

static sort_callback<hsapi::PartialTitle> get_sort_callback(SortDirection dir, SortMethod method)
{
	if (method == SortMethod::none)
		return nullptr;

	switch(dir)
	{
	case SortDirection::ascending:
		switch(method)
		{
		case SortMethod::alpha: return sort_alpha_asc;
		case SortMethod::tid: return sort_tid_asc;
		case SortMethod::size: return sort_size_asc;
		case SortMethod::downloads: return sort_downloads_asc;
		case SortMethod::id: return sort_id_asc;
		case SortMethod::none: return nullptr;
		}
		break;
	case SortDirection::descending:
		switch(method)
		{
		case SortMethod::alpha: return sort_alpha_desc;
		case SortMethod::tid: return sort_tid_desc;
		case SortMethod::size: return sort_size_desc;
		case SortMethod::downloads: return sort_downloads_desc;
		case SortMethod::id: return sort_id_desc;
		case SortMethod::none: return nullptr;
		}
		break;
	}
	/* how does this happen?! all i know is it does */
	fix_sort_settings();
	return sort_alpha_asc;
//	panic("invalid sort method/direction");
}

static void sort_if(std::vector<hsapi::PartialTitle> &to_sort, SortMethod method, SortDirection dir) {
	sort_callback<hsapi::PartialTitle> sort_cb = get_sort_callback(dir, method);
	if (sort_cb != nullptr)
		std::sort(to_sort.begin(), to_sort.end(), sort_cb);
}

#if 0
struct hsApiImageLoader {
	using DataType = const hsapi::PartialTitle&;
	using DataRefType = const hsapi::PartialTitle&;

	using Provider = ui::PassiveImageProvider<hsApiImageLoader>;

	static void batch_load(std::list<size_t> ids, Provider::Context& ctx)
	{
		C2D_Image img;
		int x, y;

		/* TODO: Convert this to an api fetch */
		for(size_t id : ids)
		{
			if(ctx.should_die())
				break;

			hsapi::hid hid = ctx.data_for(id).id;
			std::string path = "romfs:/test-icons/" + std::to_string(hid) + ".png";

			u8 *bitmap = stbi_load(path.c_str(), &x, &y, NULL, 4);
			if(!bitmap) continue; /* hmmmm...? */
			rgba_to_abgr((u32 *) bitmap, x, y);
			load_abgr8(&img, (u32 *) bitmap, x, y);
			ctx.loaded_for(id, img);
			free(bitmap);
		}
	}

	void unload(C2D_Image img)
	{
		delete_image(img);
	}
};

hsapi::hid next::sel_icon_title(std::vector<hsapi::PartialTitle>& titles, const hsapi::IndexCategory& cat, const hsapi::IndexSubcategory& subcat)
{
	using IconGridType = ui::IconGrid<hsApiImageLoader::Provider>;

	hsapi::hid ret = next_title_back;
	ui::I18NEnabledRenderQueue queue;
	ui::TitleMeta *meta;
	IconGridType *grid;

	titles.erase(titles.begin() + 10, titles.end());

	ui::builder<ui::TitleMeta>(ui::Screen::bottom, titles[0])
		.add_to(&meta, queue);

	/* TODO: Sorting, proper icon fetching, implementing load_more, saving position */

	ui::builder<IconGridType>(ui::Screen::top)
		.x(20.0f)
		.y(40.0f)
		.when_more_requested([cat, subcat](IconGridType *grid) -> bool {
			return false;
		})
		.when_selected([&ret](IconGridType *grid, size_t i, u32 keys) -> bool {
			const hsapi::PartialTitle& my_title = grid->provider_at(i).data();
			if(keys & KEY_A)          ret = my_title.id;
			else if(keys & KEY_B)     ret = next_title_back;
			else if(keys & KEY_START) ret = next_title_exit;
			else if(keys & KEY_Y)
			{
				ui::RenderQueue::global()->render_and_then([my_title]() -> void {
					queue_add(my_title.id);
				});
				return true;
			}
			return false;
		})
		.when_changed([meta](IconGridType *grid, size_t i) -> void {
			const hsapi::PartialTitle& my_title = grid->provider_at(i).data();
			meta->set_title(my_title);
		})
		.buttons(KEY_B | KEY_Y | KEY_START)
		.automatically_arranged()
		.selection()
		.icon_dimensions(48)
		.add_to(&grid, queue);

	grid->loader().set_loading_image(ui::SpriteStore::get_by_id((ui::sprite) next_loadicon48x48_idx).image);

	for(hsapi::PartialTitle& title : titles)
	{
		size_t id = grid->emplace_back(title);
		grid->loader().add_to_batch(id);
	}
	grid->loader().process_batch();

	u8 statusflags = make_status_line_clear();
	queue->render_finite();
	restore_status_line(statusflags);

	return ret;
}
#endif

hsapi::hid next::sel_title(std::vector<hsapi::PartialTitle>& titles, struct title_reenter_data *rdata, bool visited)
{
	using list_t = ui::List<hsapi::PartialTitle>;

	ui::prev_desc desc = ui::set_desc(str::select_title);
	bool focus = ui::set_focus(false);
	hsapi::hid ret = next_title_back;

	SortDirection dir = SETTING_DEFAULT_SORTDIRECTION;
	SortMethod sortm = SETTING_DEFAULT_SORTMETHOD;
	size_t cursor = 0;

	if(visited && rdata)
	{
		/* we need to copy the state */
		dir = rdata->dir;
		sortm = rdata->sortm;
		cursor = rdata->cursor;
	}
	else
		sort_if(titles, sortm, dir);

	panic_assert(titles.size() > cursor, "invalid cursor position");

	ui::I18NEnabledRenderQueue queue;

	ui::TitleMeta *meta;
	list_t *list;

	ui::builder<ui::TitleMeta>(ui::Screen::bottom, titles[cursor])
		.add_to(&meta, queue);

	ui::builder<list_t>(ui::Screen::top, &titles)
		.to_string([](const hsapi::PartialTitle& title) -> std::string {
			std::string label = hsapi::title_name(title);
			/* Append size badge when available and non-zero */
			if(title.size > 0)
			{
				label += "  ";
				label += ui::human_readable_size_block<u64>(title.size);
			}
			return label;
		})
		.when_select([&ret](list_t *self, size_t i, u32 kDown) -> bool {
			ret = self->at(i).id;
			if(kDown & KEY_B) ret = next_title_back;
			if(kDown & KEY_START) ret = next_title_exit;
			if(kDown & KEY_Y)
			{
				ui::RenderQueue::global()->render_and_then([ret]() -> void {
					queue_add(ret);
				});
				return true;
			}
			return false;
		})
		.when_change([meta](list_t *self, size_t i) -> void {
			meta->set_title(self->at(i));
		})
		.buttons(KEY_B | KEY_Y | KEY_START)
		.x(5.0f).y(25.0f)
		.add_to(&list, queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_L)
		.when_kdown([list, &dir, &sortm, &titles, meta](u32) -> bool {
			ui::RenderQueue::global()->render_and_then([list, &dir, &sortm, &titles, meta]() -> void {
				sortm = settings_sort_switch();
#if 0
				hsapi::hid curId = titles[list->get_pos()].id;
#endif
				sort_if(titles, sortm, dir);
				list->update();
#if 0
				auto it = std::find(titles.begin(), titles.end(), curId);
				panic_assert(it != titles.end(), "failed to find previously selected title");
				list->set_pos(it - titles.begin());
				meta->set_title(*it);
#endif
				list->set_pos(0);
				meta->set_title(titles[0]);
			});
			return true;
		}).add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_R)
		.when_kdown([list, &dir, &sortm, &titles, meta](u32) -> bool {
			ui::RenderQueue::global()->render_and_then([list, &dir, &sortm, &titles, meta]() -> void {
				dir = dir == SortDirection::ascending ? SortDirection::descending : SortDirection::ascending;
#if 0
				hsapi::hid curId = titles[list->get_pos()].id;
#endif
				sort_if(titles, sortm, dir);
				list->update();
#if 0
				auto it = std::find(titles.begin(), titles.end(), curId);
				panic_assert(it != titles.end(), "failed to find previously selected title");
				list->set_pos(it - titles.begin());
				meta->set_title(*it);
#endif
				list->set_pos(0);
				meta->set_title(titles[0]);
			});
			return true;
		}).add_to(queue);

	list->set_pos(cursor);
	queue.render_finite();

	/* we always want to store into rdata */
	if(rdata)
	{
		rdata->cursor = list->get_pos();
		rdata->sortm = sortm;
		rdata->dir = dir;
	}

	ui::set_focus(focus);
	ui::set_desc(desc);
	return ret;
}

void next::maybe_install_title(std::vector<hsapi::PartialTitle>& titles)
{
	next::title_reenter_data rdata;
	bool first = true;
	do {
		hsapi::hid id = next::sel_title(titles, &rdata, !first);
		first = false;

		if(id == next_title_exit || id == next_title_back)
			break;

		hsapi::Title meta;
		if(show_extmeta_lazy(titles, id, &meta))
			install::gui::hs_cia(meta);
	} while(true);
}
