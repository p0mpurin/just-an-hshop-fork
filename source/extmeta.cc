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

#include <algorithm>

#include <widgets/meta.hh>
#include <ui/base.hh>

#include "image_ldr.hh"
#include "extmeta.hh"
#include "installgui.hh"
#include "thread.hh"
#include "hsapi.hh"
#include "queue.hh"
#include "panic.hh"
#include "i18n.hh"
#include "log.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <3rd/stb_image.h>


enum class extmeta_return { yes, no, none };

template <typename TTitle = hsapi::PartialTitle>
static void show_preview(const TTitle& title)
{
	std::string png_data;
	if(R_FAILED(hsapi::call(hsapi::get_theme_preview_png, png_data, (hsapi::hid) title.id)))
		return;
	int x, y;
	u8 *bitmap = stbi_load_from_memory((const u8 *) png_data.data(), png_data.size(), &x, &y, NULL, 4);
	/* invalid file */
	if(!bitmap || x < ui::dimensions::width_top || y != ui::dimensions::height * 2)
	{
		ui::notice(str::invalid_theme_preview);
		return;
	}
	u32 bottom_offset = 4 * x * (y / 2);

	rgba_to_abgr((u32 *) bitmap, x, y);

	C2D_Image bottom, top;
	load_abgr8(&top, (u32 *) bitmap, x, ui::dimensions::height);
	load_abgr8(&bottom, (u32 *) (bitmap + bottom_offset), x, ui::dimensions::height);

	ui::I18NEnabledRenderQueue queue;
	ui::builder<ui::Sprite>(ui::Screen::top, ui::Sprite::image, (u32) &top)
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.add_to(queue);
	ui::builder<ui::Sprite>(ui::Screen::bottom, ui::Sprite::image, (u32) &bottom)
		.x(ui::layout::center_x)
		.y(ui::layout::center_y)
		.add_to(queue);

	ui::Keys keys = ui::RenderQueue::get_keys();
	while(queue.render_exclusive_frame(keys) && !keys.kDown)
		keys = ui::RenderQueue::get_keys();

	delete_image(bottom);
	delete_image(top);
	free(bitmap);
}

/* don't call with exmeta_return::none */
static bool to_bool(extmeta_return r)
{
	return r == extmeta_return::yes;
}

template <typename TTitle = hsapi::PartialTitle>
static extmeta_return extmeta(ui::I18NEnabledRenderQueue& queue, const TTitle& base, const std::string& version_s, const std::string& prodcode_s)
{
	extmeta_return ret = extmeta_return::none;
	ui::Text *press_to_install;
	ui::Text *prodcode;
	ui::Text *version;
	ui::Text *name_text;
	ui::Text *queue_hint;

	ui::builder<ui::Text>(ui::Screen::top, str::press_to_install)
		.size(0.50f, 0.50f)
		.x(ui::layout::center_x)
		.max_width(382.0f)
		.wrap()
		.add_to(&press_to_install, queue);
	/* Reserve the lower status strip for SD/NAND/network indicators. */
	press_to_install->set_y(194.0f);

	/***
	 * name (wrapped)
	 * alt_name (maybe) (wrapped)
	 * virtual console type (maybe)
	 * category -> subcategory
	 *
	 * "Press a to install, b to not"
	 * =======================
	 * version     prod
	 * tid
	 * size
	 * landing id
	 ***/

	/* name */
	ui::builder<ui::Text>(ui::Screen::top, str::name)
		.size(0.40f)
		.x(9.0f)
		.y(25.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, base.name)
		.size(0.50f)
		.x(9.0f)
		.y(36.0f)
		.max_width(382.0f)
		.wrap()
		.add_to(&name_text, queue);
	/* Long localized titles used to run directly into the alternate-name row. */
	if(name_text->height() > 42.0f)
		name_text->resize(0.42f, 0.42f);

	ui::Text *alt_text = nullptr;
	ui::Text *alt_text_label = nullptr;
	u32 alt_idx = base.preferred_alt_idx;
	ui::WidgetGroup under_alt;

	/* alternative name */
	if(base.alt.size())
	{
		auto alt_label_builder =
			base.alt_names.size() > 1 ?
				ui::builder<ui::Text>(ui::Screen::top, PSTRING(alt_name_n_of_n, alt_idx + 1, base.alt_names.size())) :
				ui::builder<ui::Text>(ui::Screen::top, str::alt_name);

		alt_label_builder
			.size(0.40f)
			.x(9.0f)
			.y(91.0f)
			.add_to(&alt_text_label, queue);
		ui::builder<ui::Text>(ui::Screen::top, base.alt)
			.size(0.40f)
			.x(9.0f)
			.y(103.0f)
			.max_width(382.0f)
			.wrap()
			.add_to(&alt_text, queue);
		if(alt_text->height() > 35.0f)
			alt_text->resize(0.40f, 0.40f);
	}


	/* virtual console type */
	const char *vc_type;
	switch((base.flags >> hsapi::VCType::shift) & hsapi::VCType::mask)
	{
	case hsapi::VCType::gb: vc_type = "Game Boy"; break;
	case hsapi::VCType::gbc: vc_type = "Game Boy Color"; break;
	case hsapi::VCType::gba: vc_type = "Game Boy Advance"; break;
	case hsapi::VCType::nes: vc_type = hsapi::subcategory(base.cat, base.subcat).name == REGION_JAPAN ? "Famicom" : "Nintendo Entertainment System"; break;
	case hsapi::VCType::snes: vc_type = hsapi::subcategory(base.cat, base.subcat).name == REGION_JAPAN ? "Super Famicom" : "Super Nintendo Entertainment System"; break;
	case hsapi::VCType::gamegear: vc_type = "Game Gear"; break;
	case hsapi::VCType::pcengine: vc_type = hsapi::subcategory(base.cat, base.subcat).name == REGION_USA ? "TurboGrafx-16" : "PC Engine"; break;
	case hsapi::VCType::none:
	default:
		vc_type = nullptr;
	}


	if(vc_type)
	{
		ui::builder<ui::Text>(ui::Screen::top, str::virtual_console_type)
			.size(0.40f)
			.x(205.0f)
			.y(157.0f)
			.add_to(queue);
		ui::builder<ui::Text>(ui::Screen::top, vc_type)
			.size(0.43f)
			.x(205.0f)
			.y(169.0f)
			.max_width(186.0f)
			.wrap()
			.add_to(queue);
	}

	/* category -> subcategory */
	ui::builder<ui::Text>(ui::Screen::top, str::category)
		.size(0.40f)
		.x(9.0f)
		.y(157.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, hsapi::format_category_and_subcategory(base.cat, base.subcat))
		.size(0.43f)
		.x(9.0f)
		.y(169.0f)
		.max_width(vc_type ? 186.0f : 382.0f)
		.wrap()
		.add_to(queue);

	/* Button hint add to queue */
	ui::builder<ui::Text>(ui::Screen::bottom, str::hint_add_queue)
		.x(9.0f).y(7.0f)
		.size(0.40f)
		.add_to(&queue_hint, queue);

	ui::builder<ui::Text>(ui::Screen::bottom, UI_GLYPH_R " Network test")
		.x(195.0f).y(218.0f)
		.size(0.40f)
		.add_to(queue);

	/* only applies to themes */
	if(hsapi::category(base.cat).name == THEMES_CATEGORY)
	{
		/* Button hint preview theme */
		ui::builder<ui::Text>(ui::Screen::bottom, str::hint_preview_theme)
			.right(queue_hint, 7.0f).y(7.0f)
			.size(0.40f)
			.max_width(155.0f)
			.add_to(queue);

		ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_X)
			.when_kdown([&base](u32) -> bool { ui::RenderQueue::global()->render_and_then([&base]() -> void {
					show_preview(base);
				}); return true; })
			.add_to(queue);
	}

	/* version */
	ui::builder<ui::Text>(ui::Screen::bottom, version_s)
		.size(0.48f)
		.x(9.0f)
		.y(42.0f)
		.max_width(140.0f)
		.add_to(&version, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::version)
		.size(0.40f)
		.x(9.0f)
		.y(29.0f)
		.add_to(queue);

	/* product code */
	ui::builder<ui::Text>(ui::Screen::bottom, prodcode_s)
		.size(0.43f)
		.x(163.0f)
		.y(42.0f)
		.max_width(148.0f)
		.wrap()
		.add_to(&prodcode, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::prodcode)
		.size(0.40f)
		.x(163.0f)
		.y(29.0f)
		.add_to(queue);

	hsapi::hsize title_size = base.size;

	/* size */
	ui::builder<ui::Text>(ui::Screen::bottom, ui::human_readable_size_block<hsapi::hsize>(title_size))
		.manual_i18n_update([title_size](ui::Text *t, lang::type) -> void {
			t->set_text(ui::human_readable_size_block<hsapi::hsize>(title_size));
		})
		.size(0.48f)
		.x(9.0f)
		.y(91.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::size)
		.size(0.40f)
		.x(9.0f)
		.y(78.0f)
		.add_to(queue);

	/* title id */
	ui::builder<ui::Text>(ui::Screen::bottom, base.tid.to_string())
		.size(0.48f)
		.x(9.0f)
		.y(140.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::tid)
		.size(0.40f)
		.x(9.0f)
		.y(127.0f)
		.add_to(queue);

	/* landing id */
	ui::builder<ui::Text>(ui::Screen::bottom, std::to_string(base.id))
		.size(0.48f)
		.x(9.0f)
		.y(189.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::landing_id)
		.size(0.40f)
		.x(9.0f)
		.y(176.0f)
		.add_to(queue);

	/* button actions */

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_B)
		.when_kdown([&ret](u32) -> bool { ret = extmeta_return::no; return false; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_A)
		.when_kdown([&ret](u32) -> bool { ret = extmeta_return::yes; return false; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_Y)
		.when_kdown([&base](u32) -> bool { ui::RenderQueue::global()->render_and_then([&base]() -> void {
				queue_add(base.id, true);
			}); return true; })
		.add_to(queue);

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_R)
		.when_kdown([&base](u32) -> bool {
			ui::RenderQueue::global()->render_and_then([&base]() -> void {
				install::gui::network_benchmark(base.id, hsapi::title_name(base));
			});
			return true;
		})
		.add_to(queue);

	if (base.alt_names.size() > 1) {
		ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_DLEFT | KEY_CPAD_LEFT)
			.when_kdown([&base, &alt_idx, &under_alt, alt_text, alt_text_label](u32) -> bool {
				if (alt_idx == 0)
					alt_idx = base.alt_names.size() - 1;
				else --alt_idx;

				alt_text->set_text(base.alt_names[alt_idx]);
				alt_text_label->set_text(PSTRING(alt_name_n_of_n, alt_idx + 1, base.alt_names.size()));
				return true;
			})
			.add_to(queue);

		ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_DRIGHT | KEY_CPAD_RIGHT)
			.when_kdown([&base, &alt_idx, &under_alt, alt_text, alt_text_label](u32) -> bool {
				if (alt_idx == base.alt_names.size() - 1)
					alt_idx = 0;
				else ++alt_idx;

				alt_text->set_text(base.alt_names[alt_idx]);
				alt_text_label->set_text(PSTRING(alt_name_n_of_n, alt_idx + 1, base.alt_names.size()));
				return true;
			})
			.add_to(queue);
	}

	queue.render_finite();
	return ret;
}

static extmeta_return extmeta(const hsapi::Title& title)
{
	ui::I18NEnabledRenderQueue queue;
	return extmeta(queue, title, hsapi::parse_vstring(title.version) + " (" + std::to_string(title.version) + ")", title.prod);
}

bool show_extmeta_lazy(const hsapi::PartialTitle& base, hsapi::Title *full)
{
	ui::prev_desc desc = ui::set_desc(str::more_about_content);
	ui::I18NEnabledRenderQueue queue;
	bool ret = true;

	hsapi::Title stack_title;
	if(!full) full = &stack_title;

	hsapi::hid id = base.id;

	ctr::thread<ui::I18NEnabledRenderQueue&, hsapi::hid, hsapi::Title&> th([](ui::I18NEnabledRenderQueue& queue, hsapi::hid id, hsapi::Title& full) -> void {
		/* Only force stop the queue if we have something loading */
		if(R_SUCCEEDED(hsapi::title_meta(full, id)))
			queue.signal(ui::RenderQueue::signal_cancel);
	}, -1, queue, (hsapi::hid) id, *full);


	extmeta_return res = extmeta(queue, base, STRING(loading), STRING(loading));
	/* second thread returned more data */
	if(res == extmeta_return::none)
	{
		dlog("Lazy load finished before choice was made.");
		queue.clear();

		res = extmeta(queue, base,
			hsapi::parse_vstring(full->version) + " (" + std::to_string(full->version) + ")",
			full->prod);
	}
	ret = to_bool(res);

	/* At this point we're done rendering and
	 * waiting for the *fetching* of the full data
	 * and *setting* of the renderqueue callback */
	th.join();

	ui::set_desc(desc);
	return ret;
}

bool show_extmeta_lazy(std::vector<hsapi::PartialTitle>& titles, hsapi::hid id, hsapi::Title *full)
{
	std::vector<hsapi::PartialTitle>::iterator it =
		std::find_if(titles.begin(), titles.end(), [id](const hsapi::PartialTitle& t) -> bool {
			return t.id == id;
		});

	panic_assert(it != titles.end(), "Could not find id in vector");
	return show_extmeta_lazy(*it, full);
}

bool show_extmeta(const hsapi::Title& title)
{
	ui::prev_desc desc = ui::set_desc(str::more_about_content);
	bool ret = to_bool(extmeta(title));
	ui::set_desc(desc);
	return ret;
}
