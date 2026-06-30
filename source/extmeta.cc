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
#include <cstring>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include <3ds/services/pmapp.h>
}

#include <widgets/meta.hh>
#include <ui/base.hh>
#include <ui/confirm.hh>

#include "image_ldr.hh"
#include "extmeta.hh"
#include "installgui.hh"
#include "install.hh"
#include "thread.hh"
#include "hsapi.hh"
#include "queue.hh"
#include "panic.hh"
#include "i18n.hh"
#include "log.hh"
#include "settings.hh"

#define STB_IMAGE_IMPLEMENTATION
#include <3rd/stb_image.h>


enum class extmeta_return { yes, no, none };

#define RUNEFETCH_DIR        "/3ds/Rune3DS/runefetch"
#define RUNEFETCH_JOBS_DIR   "/3ds/Rune3DS/runefetch/jobs"
#define RUNEFETCH_CANCEL_DIR "/3ds/Rune3DS/runefetch/cancel"
#define RUNEFETCH_STATE_DIR  "/3ds/Rune3DS/runefetch/state"
#define RUNEFETCH_TITLE_ID   0x0004013000C0FE02ULL

static Result runefetch_launch_sysmodule()
{
	Result res = pmAppInit();
	if(R_FAILED(res))
		return res;

	FS_ProgramInfo program_info {};
	program_info.programId = RUNEFETCH_TITLE_ID;
	program_info.mediaType = MEDIATYPE_NAND;

	res = PMAPP_LaunchTitle(&program_info, PMLAUNCHFLAG_LOAD_DEPENDENCIES);
	pmAppExit();
	return res;
}

template <typename TTitle>
static std::string runefetch_job_basename(const TTitle& title)
{
	std::string source = title.tid.to_string();
	if(source.empty())
		source = std::to_string(title.id);

	std::string out;
	out.reserve(source.size());
	for(char c : source)
	{
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') || c == '-' || c == '_';
		if(ok) out += c;
	}
	if(out.empty())
		out = "job-" + std::to_string(title.id);
	return out;
}

static std::string runefetch_escape_value(const std::string& value)
{
	std::string out;
	out.reserve(value.size());
	for(char c : value)
	{
		if(c == '\n' || c == '\r')
			out += ' ';
		else
			out += c;
	}
	return out;
}

template <typename TTitle>
static std::string runefetch_job_path(const TTitle& title)
{
	return std::string(RUNEFETCH_JOBS_DIR) + "/" + runefetch_job_basename(title) + ".job";
}

template <typename TTitle>
static std::string runefetch_cancel_path(const TTitle& title)
{
	return std::string(RUNEFETCH_CANCEL_DIR) + "/" + runefetch_job_basename(title) + ".cancel";
}

static bool runefetch_path_exists(const std::string& path)
{
	return access(path.c_str(), F_OK) == 0;
}

static bool runefetch_job_allows_cancel(const std::string& path)
{
	FILE *f = fopen(path.c_str(), "r");
	if(!f)
		return false;

	char line[128];
	while(fgets(line, sizeof(line), f))
	{
		if(strncmp(line, "mode=", 5) != 0)
			continue;

		char *mode = line + 5;
		mode[strcspn(mode, "\r\n")] = '\0';
		fclose(f);
		return strcmp(mode, "cache") == 0 || strcmp(mode, "download_only") == 0;
	}

	fclose(f);
	return false;
}

template <typename TTitle>
static Result runefetch_write_job(const TTitle& title)
{
	std::string url;
	Result res = hsapi::call(hsapi::get_download_link, url, (hsapi::hid) title.id);
	if(R_FAILED(res))
		return res;

	mkdir("/3ds", 0777);
	mkdir("/3ds/Rune3DS", 0777);
	mkdir(RUNEFETCH_DIR, 0777);
	mkdir(RUNEFETCH_JOBS_DIR, 0777);
	mkdir(RUNEFETCH_CANCEL_DIR, 0777);
	mkdir(RUNEFETCH_STATE_DIR, 0777);

	std::string base = runefetch_job_basename(title);
	std::string tmp_path = std::string(RUNEFETCH_JOBS_DIR) + "/" + base + ".tmp";
	std::string job_path = runefetch_job_path(title);

	remove(runefetch_cancel_path(title).c_str());

	FILE *f = fopen(tmp_path.c_str(), "w");
	if(!f)
		return MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, 0x70);

	fprintf(f, "version=1\n");
	fprintf(f, "id=%lu\n", (unsigned long) title.id);
	fprintf(f, "title_id=%s\n", title.tid.to_string().c_str());
	fprintf(f, "name=%s\n", runefetch_escape_value(hsapi::title_name(title)).c_str());
	fprintf(f, "url=%s\n", url.c_str());
	fprintf(f, "size=%llu\n", (unsigned long long) title.size);
	fprintf(f, "mode=%s\n", ISET_RUNEFETCH_CACHE ? "cache" : "stream_install");
	fclose(f);

	remove(job_path.c_str());
	if(rename(tmp_path.c_str(), job_path.c_str()) != 0)
		return MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 0x71);

	return 0;
}

template <typename TTitle>
static Result runefetch_cancel_job(const TTitle& title)
{
	mkdir("/3ds", 0777);
	mkdir("/3ds/Rune3DS", 0777);
	mkdir(RUNEFETCH_DIR, 0777);
	mkdir(RUNEFETCH_CANCEL_DIR, 0777);

	std::string cancel_path = runefetch_cancel_path(title);
	FILE *f = fopen(cancel_path.c_str(), "w");
	if(!f)
		return MAKERESULT(RL_PERMANENT, RS_NOTFOUND, RM_APPLICATION, 0x72);

	fprintf(f, "title_id=%s\n", title.tid.to_string().c_str());
	fprintf(f, "id=%lu\n", (unsigned long) title.id);
	fclose(f);

	remove(runefetch_job_path(title).c_str());
	return 0;
}

template <typename TTitle>
static void runefetch_enqueue_background(const TTitle& title)
{
	std::string job_path = runefetch_job_path(title);
	if(runefetch_path_exists(job_path))
	{
		if(!runefetch_job_allows_cancel(job_path))
		{
			ui::notice("RuneFetch already has a stream install job for this title.\n\nStream installs cannot be safely canceled after install starts. Reboot the console if you need to stop it.");
			return;
		}

		if(!ui::Confirm::exec(
			"RuneFetch already has a cached download job for this title.\n\nCancel it?",
			"RuneFetch job", false))
			return;

		Result cancel_res = runefetch_cancel_job(title);
		if(R_FAILED(cancel_res))
		{
			error_container err = get_error(cancel_res);
			report_error(err, "RuneFetch cancel job for content ID " + std::to_string(title.id));
			handle_error(err);
			return;
		}

		ui::notice("RuneFetch cancel requested.\n\nIf the sysmodule is already running, it will stop at the next safe checkpoint.");
		return;
	}

	const bool cache_mode = ISET_RUNEFETCH_CACHE;
	const char *confirm_message = cache_mode
		? "Queue a RuneFetch background download?\n\nRuneFetch will cache the CIA so Rune3DS stays usable while it runs. Install it later from FBI."
		: "Queue a RuneFetch stream install?\n\nThis is experimental and mainly tested on New 3DS/New 2DS. It downloads and installs through AM in the background, but it cannot be safely canceled after install starts. Reboot the console if you need to stop it.";
	if(!ui::Confirm::exec(confirm_message, "RuneFetch", true))
		return;

	Result res = runefetch_write_job(title);
	if(R_FAILED(res))
	{
		error_container err = get_error(res);
		report_error(err, "RuneFetch background job for content ID " + std::to_string(title.id));
		handle_error(err);
		return;
	}

	if(ISET_RUNEFETCH_AUTO_LAUNCH)
	{
		res = runefetch_launch_sysmodule();
		if(R_FAILED(res))
		{
			error_container err = get_error(res);
			report_error(err, "RuneFetch sysmodule launch");
			handle_error(err);
			return;
		}
	}

	if(cache_mode)
		ui::notice(ISET_RUNEFETCH_AUTO_LAUNCH
			? "RuneFetch download queued.\n\nYou can keep browsing Rune3DS, press HOME, or play another title. The notification LED shows progress.\n\nWhen it finishes, install the cached CIA from FBI."
			: "RuneFetch download job written.\n\nAuto-launch is disabled, so RuneFetch will pick it up the next time the sysmodule runs.");
	else
		ui::notice(ISET_RUNEFETCH_AUTO_LAUNCH
			? "RuneFetch stream install queued.\n\nYou can press HOME or play another title. Stream mode cannot be safely canceled after install starts; reboot the console if you need to stop it."
			: "RuneFetch stream install job written.\n\nAuto-launch is disabled, so RuneFetch will pick it up the next time the sysmodule runs.");
}

class ExtMetaBottomPanel : public ui::BaseWidget
{ UI_WIDGET("ExtMetaBottomPanel")
public:
	float width() override { return ui::screen_width(this->screen); }
	float height() override { return ui::screen_height(); }

	bool render(ui::Keys&) override
	{
		C2D_DrawRectSolid(8.0f, 10.0f, -0.33f, 304.0f, 194.0f, C2D_Color32(0, 0, 0, 190));
		C2D_DrawRectSolid(8.0f, 10.0f, -0.30f, 304.0f, 1.0f, C2D_Color32(255, 255, 255, 34));
		C2D_DrawRectSolid(8.0f, 203.0f, -0.30f, 304.0f, 1.0f, C2D_Color32(255, 255, 255, 18));
		C2D_DrawRectSolid(18.0f, 72.0f, -0.28f, 282.0f, 1.0f, C2D_Color32(255, 255, 255, 16));
		C2D_DrawRectSolid(18.0f, 124.0f, -0.28f, 282.0f, 1.0f, C2D_Color32(255, 255, 255, 12));
		return true;
	}
};

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

	ui::builder<ExtMetaBottomPanel>(ui::Screen::bottom)
		.add_to(queue);

	ui::builder<ui::Text>(ui::Screen::top, str::press_to_install)
		.size(0.48f, 0.48f)
		.x(ui::layout::center_x)
		.y(18.0f)
		.max_width(360.0f)
		.wrap()
		.add_to(&press_to_install, queue);

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
	ui::builder<ui::Text>(ui::Screen::top, base.name)
		.size(0.58f)
		.x(ui::layout::center_x)
		.y(48.0f)
		.max_width(360.0f)
		.wrap()
		.add_to(&name_text, queue);
	/* Long localized titles used to run directly into the alternate-name row. */
	if(name_text->height() > 42.0f)
		name_text->resize(0.46f, 0.46f);

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
			.x(ui::layout::center_x)
			.y(102.0f)
			.max_width(360.0f)
			.add_to(&alt_text_label, queue);
		ui::builder<ui::Text>(ui::Screen::top, base.alt)
			.size(0.40f)
			.x(ui::layout::center_x)
			.y(115.0f)
			.max_width(360.0f)
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
			.x(214.0f)
			.y(154.0f)
			.max_width(172.0f)
			.add_to(queue);
		ui::builder<ui::Text>(ui::Screen::top, vc_type)
			.size(0.43f)
			.x(214.0f)
			.y(166.0f)
			.max_width(172.0f)
			.wrap()
			.add_to(queue);
	}

	/* category -> subcategory */
	ui::builder<ui::Text>(ui::Screen::top, str::category)
		.size(0.40f)
		.x(vc_type ? 14.0f : ui::layout::center_x)
		.y(154.0f)
		.max_width(vc_type ? 172.0f : 360.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, hsapi::format_category_and_subcategory(base.cat, base.subcat))
		.size(0.43f)
		.x(vc_type ? 14.0f : ui::layout::center_x)
		.y(166.0f)
		.max_width(vc_type ? 172.0f : 360.0f)
		.wrap()
		.add_to(queue);

	/* Button hint add to queue */
	ui::builder<ui::Text>(ui::Screen::bottom, str::hint_add_queue)
		.x(11.0f).y(214.0f)
		.size(0.40f)
		.max_width(104.0f)
		.add_to(&queue_hint, queue);

	ui::builder<ui::Text>(ui::Screen::bottom, UI_GLYPH_R " Network test")
		.x(208.0f).y(214.0f)
		.size(0.40f)
		.max_width(104.0f)
		.add_to(queue);

	ui::builder<ui::Text>(ui::Screen::bottom, UI_GLYPH_L " RuneFetch")
		.x(116.0f).y(hsapi::category(base.cat).name == THEMES_CATEGORY ? 200.0f : 214.0f)
		.size(0.40f)
		.max_width(100.0f)
		.add_to(queue);

	/* only applies to themes */
	if(hsapi::category(base.cat).name == THEMES_CATEGORY)
	{
		/* Button hint preview theme */
		ui::builder<ui::Text>(ui::Screen::bottom, str::hint_preview_theme)
			.x(116.0f).y(214.0f)
			.size(0.40f)
			.max_width(88.0f)
			.add_to(queue);

		ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_X)
			.when_kdown([&base](u32) -> bool { ui::RenderQueue::global()->render_and_then([&base]() -> void {
					show_preview(base);
				}); return true; })
			.add_to(queue);
	}

	/* version */
	ui::builder<ui::Text>(ui::Screen::bottom, version_s)
		.size(0.56f)
		.x(18.0f)
		.y(28.0f)
		.max_width(282.0f)
		.add_to(&version, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::version)
		.size(0.42f)
		.x(18.0f)
		.y(16.0f)
		.add_to(queue);

	/* product code */
	ui::builder<ui::Text>(ui::Screen::bottom, prodcode_s)
		.size(0.52f)
		.x(18.0f)
		.y(61.0f)
		.max_width(282.0f)
		.wrap()
		.add_to(&prodcode, queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::prodcode)
		.size(0.42f)
		.x(18.0f)
		.y(49.0f)
		.add_to(queue);

	hsapi::hsize title_size = base.size;

	/* size */
	ui::builder<ui::Text>(ui::Screen::bottom, ui::human_readable_size_block<hsapi::hsize>(title_size))
		.manual_i18n_update([title_size](ui::Text *t, lang::type) -> void {
			t->set_text(ui::human_readable_size_block<hsapi::hsize>(title_size));
		})
		.size(0.56f)
		.x(18.0f)
		.y(94.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::size)
		.size(0.42f)
		.x(18.0f)
		.y(82.0f)
		.add_to(queue);

	/* title id */
	ui::builder<ui::Text>(ui::Screen::bottom, base.tid.to_string())
		.size(0.52f)
		.x(18.0f)
		.y(139.0f)
		.max_width(282.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::tid)
		.size(0.42f)
		.x(18.0f)
		.y(127.0f)
		.add_to(queue);

	/* landing id */
	ui::builder<ui::Text>(ui::Screen::bottom, std::to_string(base.id))
		.size(0.52f)
		.x(18.0f)
		.y(172.0f)
		.max_width(282.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::bottom, str::landing_id)
		.size(0.42f)
		.x(18.0f)
		.y(160.0f)
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

	ui::builder<ui::ButtonCallback>(ui::Screen::top, KEY_L)
		.when_kdown([&base](u32) -> bool {
			ui::RenderQueue::global()->render_and_then([&base]() -> void {
				runefetch_enqueue_background(base);
			});
			return true;
		})
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
	bool focus = ui::set_focus(true);
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

	/* Pre-fetch the CDN download URL while the user reads the details.
	 * This saves ~500ms when they press A to install. */
	if(full->id != 0)
		install::pre_fetch_url(*full);

	ui::set_focus(focus);
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
	bool focus = ui::set_focus(true);
	bool ret = to_bool(extmeta(title));
	ui::set_focus(focus);
	ui::set_desc(desc);
	return ret;
}
