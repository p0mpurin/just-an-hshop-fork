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

#include "installgui.hh"

#include <ui/progress_bar.hh>
#include <ui/base.hh>

#include <algorithm>

#include "dmn.hh"
#include "install.hh"
#include "mng.hh"
#include "widgets/indicators.hh"
#include "find_missing.hh"
#include "lumalocale.hh"
#include "settings.hh"
#include "hsapi.hh"
#include "panic.hh"
#include "log.hh"


UI_CTHEME_GETTER(color_led_green, ui::theme::led_green_color)
UI_CTHEME_GETTER(color_led_red, ui::theme::led_red_color)
static ui::slot_color_getter slotmgr_getters[] = {
	color_led_green, color_led_red
};

static ui::SlotManager slotmgr;


void make_render_queue(ui::I18NEnabledRenderQueue& queue, ui::ProgressBar **bar,
	ui::Text **stage, const std::string& label)
{
	/* Keep the upstream download screen structure. Nocturne adds only a
	 * compact stage line and clearer speed/ETA labels. */
	ui::Text *donate;
	ui::builder<ui::Text>(ui::progloc(), str::do_donate)
		.size(0.46f)
		.x(ui::layout::center_x)
		.y(18.0f)
		.max_width(ui::screen_width(ui::progloc()) - 20.0f)
		.wrap()
		.add_to(&donate, queue);

	ui::builder<ui::Text>(ui::progloc(), "Requesting download link")
		.size(0.40f)
		.x(ui::layout::center_x)
		.y(48.0f)
		.max_width(ui::screen_width(ui::progloc()) - 20.0f)
		.wrap()
		.add_to(stage, queue);

	ui::builder<ui::ProgressBar>(ui::progloc())
		.y(105.0f)
		.use_speed()
		.add_to(bar, queue);

	if(label.size())
	{
		ui::builder<ui::Text>(ui::progloc() == ui::Screen::top ? ui::Screen::bottom : ui::Screen::top, label)
			.size(0.40f)
			.x(ui::layout::center_x)
			.y(28.0f)
			.max_width(ui::screen_width(ui::progloc() == ui::Screen::top ? ui::Screen::bottom : ui::Screen::top) - 20.0f)
			.z(ui::layer::top)
			.wrap()
			.add_to(queue);
	}

	if(!ISET_DISABLE_GRAPH)
	{
		ui::builder<ui::LatencyGraph>(ui::progloc() == ui::Screen::top ? ui::Screen::bottom : ui::Screen::top, (*bar)->speed_buffer())
			.y(ui::layout::center_y)
			.z(ui::layer::middle)
			.add_to(queue);
	}

	/* Do not source this control hint from title/theme metadata. */
	ui::builder<ui::Text>(ui::Screen::bottom, UI_GLYPH_B " (hold): Cancel Download")
		.size(0.40f)
		.x(ui::layout::center_x)
		.y(220.0f)
		.max_width(300.0f)
		.add_to(queue);

	queue.render_frame();
}

static bool ask_reinstall(bool interactive)
{
	return interactive ?
		ui::Confirm::exec(str::already_installed_reinstall) : false;
}

static void finalize_hs_install(const hsapi::Title& meta, std::vector<hsapi::RelatedFullTitle>& related, bool interactive)
{
	ui::RenderQueue::global()->find_tag<ui::FreeSpaceIndicator>(ui::tag::free_indicator)->update();

	// Prompt to ask for extra content
	if(interactive && meta.tid.can_have_missing() && !(meta.flags & hsapi::TitleFlag::skip_force_rel) && ISET_SEARCH_ECONTENT)
	{
		size_t added;
		manual_find_missing(related, added);
		if(added > 0) ui::notice(PSTRING(found_missing, added));
	}

	/* only set locale if we're interactive, otherwise the caller has to handle it themselves */
	if(interactive && luma::set_locale(meta.tid, interactive))
		luma::maybe_set_gamepatching();
}


static void finalize_install(hsapi::htid tid, bool interactive)
{
	ui::RenderQueue::global()->find_tag<ui::FreeSpaceIndicator>(ui::tag::free_indicator)->update();

	/* only set locale if we're interactive, otherwise the caller has to handle it themselves */
	if(interactive && luma::set_locale(tid, interactive))
		luma::maybe_set_gamepatching();
}

static bool generic_check_for_base(ctr::title_id tid, bool interactive)
{
	/* user cannot tell us if they want to continue */
	if (!interactive)
		return true;

	/* user has disabled "check for missing base title" */
	if (!ISET_WARN_NO_BASE)
		return true;

	/* the title is a base title */
	if (tid.is_base_tid())
		return true;

	/* the title is not a base title but the base title is already installed */
	if (ctr::mng::title_exists_anywhere(tid.base_tid())) /* anywhere because gamecards are a thing too */
		return true;

	/* the title is not a base title and the base title for it isn't installed, possibly a mistake.
	   warn the user about this */
	return ui::Confirm::exec(str::install_no_base);
}

static Result hs_check_for_base(const hsapi::Title& meta, std::vector<hsapi::RelatedFullTitle>& out_related, bool interactive)
{
	/* user cannot tell us if they want to continue */
	if (!interactive)
		return 1;

	/* user has disabled "check for missing base title" */
	if (!ISET_WARN_NO_BASE)
		return 1;

	if (meta.tid.is_base_tid()) /* title itself is a base, not installing related base titles should be fine */
		return 1;

	hsapi::hid title_hid = meta.id;
	/* query the relations */
	Result ret = hsapi::scall(hsapi::single_relations, out_related, std::move(title_hid));
	if (R_FAILED(ret)) return ret;

	/* there are no relations; meaning there are no possible base titles to check for */
	if (!out_related.size())
		return 1;

	bool missing_base = std::find_if(out_related.begin(), out_related.end(),
		[](const hsapi::RelatedFullTitle& rtl) -> bool {
			/* true: missing base, false: have base */
			return rtl.relation == hsapi::TitleRelationType::Base &&
					rtl.tid.is_base_tid() &&
					!ctr::mng::title_exists_anywhere(rtl.tid);
		}) != out_related.end();

	/* we are not missing any base titles. */
	if (!missing_base) return 1;

	/* we are missing base titles. this is usually a mistake on the user end. */
	return ui::Confirm::exec(str::install_no_base);
	/* 0: do not continue, 1: continue, anything else: error */
}

Result install::gui::net_cia(const std::string& url, ctr::title_id tid, bool interactive, bool defaultReinstallable, bool hsapiEnabled, bool doVersionCheck)
{
	if(!generic_check_for_base(tid, interactive))
		return APPERR_NO_BASE;

	bool focus = ui::set_focus(true);
	ui::ProgressBar *bar;
	ui::Text *stage;
	ui::I18NEnabledRenderQueue queue;

	make_render_queue(queue, &bar, &stage, "");

	bool shouldReinstall = defaultReinstallable;
	Result res = 0;

start_install:
	res = install::net_cia(makeurlwrap(url), tid, [&queue, &bar, stage](u64 now, u64 total) -> void {
		stage->set_text(now ? "Downloading + installing" : "Preparing installation");
		bar->update(now, total);
		bar->activate();
		queue.render_frame();
	}, shouldReinstall, hsapiEnabled, doVersionCheck);

	if(res == APPERR_NOREINSTALL)
	{
		if((shouldReinstall = ask_reinstall(interactive)))
			goto start_install;
	}

	if(R_FAILED(res) && res != APPERR_CANCELLED)
	{
		error_container err = get_error(res);
		report_error(err, "User was installing from " + url);
		if(interactive) handle_error(err);
	}
	else if(R_SUCCEEDED(res))
	{
		stage->set_text("Finalizing installation");
		queue.render_frame();
		finalize_install(tid, interactive);
	}

	ui::set_focus(focus);
	return res;
}

Result install::gui::hs_cia(const hsapi::Title& meta, bool interactive, bool defaultReinstallable, const std::string& label)
{
	std::vector<hsapi::RelatedFullTitle> relatedTitles;
	if(!hs_check_for_base(meta, relatedTitles, interactive))
		return APPERR_NO_BASE;

	bool focus = ui::set_focus(true);
	ui::ProgressBar *bar;
	ui::Text *stage;
	ui::I18NEnabledRenderQueue queue;

	make_render_queue(queue, &bar, &stage, label.size() ? label : meta.name);

	bool shouldReinstall = defaultReinstallable;
	Result res = 0;

	if(interactive && R_FAILED(res = ctr::dmn::increase_sleep_lock_ref()))
		elog("failed to acquire sleep mode lock: %08lX", res);

start_install:
	res = install::hs_cia(meta, [&queue, &bar, stage](u64 now, u64 total) -> void {
		stage->set_text(now
			? (install::is_direct_cdn_active()
				? "Direct socket + installing"
				: "Downloading + installing")
			: "Preparing installation");
		bar->update(now, total);
		bar->activate();
		queue.render_frame();
	}, shouldReinstall);

	if(res == APPERR_NOREINSTALL)
	{
		if((shouldReinstall = ask_reinstall(interactive)))
			goto start_install;
	}

	if(R_SUCCEEDED(res))
	{
		stage->set_text("Verifying + finalizing");
		queue.render_frame();
		res = ctr::mng::import_seed(meta.tid, &meta.seed);
		finalize_hs_install(meta, relatedTitles, interactive);
		if(interactive)
		{
			if(hsapi::category(meta.cat).name == THEMES_CATEGORY)
				ui::notice(str::theme_installed);
			else if(meta.flags & hsapi::TitleFlag::installer)
				ui::notice(str::file_installed);
		}
		install::gui::SuccessLED();
	}
	else if(res != APPERR_CANCELLED)
	{
		install::gui::ErrorLED();
		error_container err = get_error(res);
		report_error(err, "User was installing (" + meta.tid.to_string() + ") (" + std::to_string(meta.id) + ")");
		if(interactive) handle_error(err);
	}

	if(interactive) ctr::dmn::decrease_sleep_lock_ref();
	/* do not reset flags: we want to trigger a LED Reset after either sleep mode lift or the timeout, not after just the timeout */
	else            ui::LED::SetTimeout(time(NULL) + 2);

	ui::set_focus(focus);
	return res;
}

Result install::gui::network_benchmark(hsapi::hid id, const std::string& label)
{
	bool focus = ui::set_focus(true);
	ui::ProgressBar *bar;
	ui::Text *stage;
	ui::I18NEnabledRenderQueue queue;
	make_render_queue(queue, &bar, &stage, label);
	stage->set_text("hShop CDN network-only test");
	queue.render_frame();

	install::NetworkBenchmarkResult stats;
	Result res = install::hs_network_benchmark(id, stats,
		[&queue, &bar, stage](u64 now, u64 total) -> void {
			stage->set_text(now
				? (install::is_direct_cdn_active()
					? "Direct socket to RAM · no SD writes"
					: "Nintendo HTTP to RAM · no SD writes")
				: "Requesting authenticated CDN link");
			bar->update(now, std::min<u64>(total, 32ULL * 1024ULL * 1024ULL));
			bar->activate();
			queue.render_frame();
		});

	ui::set_focus(focus);
	if(R_SUCCEEDED(res))
	{
		char message[256];
		snprintf(message, sizeof(message),
			"hShop CDN network-only result\n\nAverage  %.2f MiB/s\nPeak       %.2f MiB/s\nData       %.1f MiB\n\nNo CIA data was installed or written to SD.",
			stats.average_mib_s, stats.peak_mib_s, stats.bytes / (1024.0f * 1024.0f));
		ui::notice(message, 42.0f);
	}
	else if(res != APPERR_CANCELLED)
	{
		error_container err = get_error(res);
		report_error(err, "User was running hShop CDN network benchmark for landing ID " + std::to_string(id));
		handle_error(err);
	}
	return res;
}

static void setled(size_t i)
{
	if(!slotmgr.is_initialized())
		slotmgr = ui::ThemeManager::global()->get_slots(nullptr, "__global_install_gui_colors", 2, slotmgr_getters);

	ilog("color: %08lX", slotmgr.get(i));

	ui::LED::Pattern pattern;
	ui::LED::Solid(&pattern, UI_LED_MAKE_ANIMATION(0, 0xFF, 0), slotmgr.get(i));
	ui::LED::SetSleepPattern(&pattern);
}

void install::gui::SuccessLED() { setled(0); }
void install::gui::ErrorLED()   { setled(1); }
