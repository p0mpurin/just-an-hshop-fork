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

#include <3ds.h>
#include <sys/stat.h>

#include <ui/popup.hh>
#include <ui/base.hh>
#include <ui/progress_bar.hh>

#include <widgets/status_line.hh>
#include <widgets/indicators.hh>
#include <widgets/konami.hh>
#include <widgets/meta.hh>

#include "audio/configuration.h"
#include "audio/cwav_reader.h"
#include "audio/player.h"
#include "installgui.hh"
#include "mng.hh"
#include "settings.hh"
#include "extmeta.hh"
#include "update.hh"
#include "thread.hh"
#include "search.hh"
#include "queue.hh"
#include "panic.hh"
#include "hsapi.hh"
#include "httpclient.hh"
#include "more.hh"
#include "next.hh"
#include "i18n.hh"
#include "log.hh"
#include "wlan.hh"

#define ENVINFO (* (u8 *) 0x1FF80014)
#define VERSION_CHECK 1
#define TIP_GIVER 0

#ifndef NOCTURNE_UPDATE_BASE
	#define NOCTURNE_UPDATE_BASE "http://nocturne.atwebpages.com"
#endif

ctr::thread<Handle &, Handle &> *wlan_thread = nullptr;
static Handle wlan_thread_exit_event = 0;
static Handle wlan_disconnect_event = 0;

#ifndef RELEASE
class FrameCounter : public ui::BaseWidget
{ UI_WIDGET("FrameCounter")
public:
	float width() override { return this->t->width(); }
	float height() override { return this->t->height(); }
	void set_x(float x) override { this->x = x; this->t->set_x(x); }
	void set_y(float y) override { this->y = y; this->t->set_y(y); }
	void resize(float x, float y) { this->t->resize(x, y); }

	void setup() override
	{
		this->t.setup(this->screen, "0 fps");
	}

	bool render(ui::Keys& k) override
	{
		time_t now = time(NULL);
		if(now != this->frames[this->i].time)
			this->switch_frame(now);
		++this->frames[this->i].frames;
		this->t->render(k);
		return true;
	}

	int fps()
	{
		return this->frames[1 - this->i].frames;
	}

private:
	struct {
		time_t time;
		int frames;
	} frames[2] = {
		{ 0, 60 },
		{ 0, 60 },
	};

	ui::ScopedWidget<ui::Text> t;
	size_t i = 0;

	void set_label(int fps)
	{
		this->t->set_text(std::to_string(fps) + " fps");
		this->t->set_x(this->x);
	}

	void switch_frame(time_t d)
	{
		this->set_label(this->frames[this->i].frames);
		this->i = 1 - this->i;
		this->frames[this->i].time = d;
		this->frames[this->i].frames = 0;
	}

};
#endif

#if TIP_GIVER
class TipGiver : public ui::BaseWidget
{ UI_WIDGET("TipGiver")
public:
	void setup()
	{
		this->frames_until_tip = this->initial_frames_until_tip();
	}

	float height() override { return 0.0f; }
	float width() override { return 0.0f; }

	bool render(ui::Keys&) override
	{
		/* don't advance if we're already using the status or installing a game */
		if(install::is_in_progress() || status_running() || !this->frames_until_tip)
			return true;

		--this->frames_until_tip;
		if(!this->frames_until_tip)
		{
			this->frames_until_tip = this->next_frames_until_tip();
			set_ticker(this->select_string());
		}
		return true;
	}

private:
	/* frames_to_seconds = frames => frames * 60 */
	/* seconds_to_frames = secs => secs / 60 */
	unsigned initial_frames_until_tip()
	{
		return 30;
	}

	unsigned next_frames_until_tip()
	{
		return ~0;
	}

	const char *select_string()
	{
		return STRING(do_donate);
	}

	unsigned frames_until_tip;

};
#endif

static void brick_negro()
{
}

#ifdef RELEASE
[[noreturn]] static void block_app(str::type header, str::type msg, Result associated_result = 0xE7E3FFFF) {
	ui::set_focus(true);

	if (associated_result != 0xE7E3FFFF && !R_SUCCEEDED(associated_result)) {
		error_container err = get_error(associated_result);
		report_error(err);
		handle_error(err);
	}

	ui::I18NEnabledRenderQueue queue;
	ui::builder<ui::Text>(ui::Screen::top, header)
		.x(ui::layout::center_x).y(45.0f)
		.wrap()
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, msg)
		.x(ui::layout::center_x).under(queue.back())
		.wrap()
		.add_to(queue);

	queue.render_finite_button(-1);

	exit(0);
}
#endif

void make_render_queue(ui::I18NEnabledRenderQueue& queue, ui::ProgressBar **bar,
	ui::Text **stage, const std::string& label);

void wlan_thread_exit() {
	if (wlan_thread) {
		svcSignalEvent(wlan_thread_exit_event);
		wlan_thread->join();
		delete wlan_thread;
	}
	svcCloseHandle(wlan_thread_exit_event);
	svcCloseHandle(wlan_disconnect_event);
	svcCloseHandle(*ctr::wlan::connect_mtx());
}

int main(int argc, char* argv[])
{
	((void) argc);
	((void) argv);

	Result res = 0xE7E37FFF;

	/* check if a new language was set, which happens only when the settings file is reset */
	bool languageDetected = ensure_settings(); /* log_init() uses settings, so we need this here */
	bool disableAutoWlan = false;

	/* write changed made to settings on app exit */
	atexit(settings_sync);

	/* initialize log subsystem */
	log_init();

	/* release resources used by log system on app exit */
	atexit(log_exit);

#ifdef RELEASE
	#define EV
#else
	#define EV "-debug"
#endif
	ilog("current 3hs version is " VVERSION EV "%s" " \"" VERSION_DESC "\"", envIsHomebrew() ? "-3dsx" : "");
#undef EV

	log_settings();

	/* initialize essential services */

	bool isLuma = false;
	res = init_services(isLuma);
	panic_assert(R_SUCCEEDED(res),
		"init_services() failed, this should **never** happen (0x" + pad8code(res) + ")");

	/* releases resources used by essential services on app exit */
	atexit(exit_services);
	
	bool avail = false;
	panic_assert(R_SUCCEEDED(AM_QueryAvailableExternalTitleDatabase(&avail)), "could not initialize SD title database");

	/* initialize theme subsystem */
	load_current_theme();
	atexit(cleanup_themes);
	panic_assert(themes().size() > 0, "failed to load any themes");

	/* initialize UI */
	panic_assert(ui::init(), "ui::init() failed, this should **never** happen");

	/* deinitialize UI on app exit */
	atexit(ui::exit);

	/* allow panic() to use graphics from this point forward */
	panic_enable_gfx();

	osSetSpeedupEnable(true); // speedup for n3dses

	/*
		if R is held while 3hs is launching, we will reset the settings and the set language to English.
		this is useful in case the user does not understand the system language and/or an improper region change
		causes old language settings to remain in the CFG system save.
	*/

	hidScanInput();
	u32 appBootKeys = hidKeysDown() | hidKeysHeld();
	if(appBootKeys & KEY_R)
	{
		reset_settings();
		languageDetected = false;
	}

	if(appBootKeys & KEY_L)
		disableAutoWlan = true;

	/* IYKYK. */
	if(get_nsettings()->lang == lang::spanish)
		brick_negro();

	/* Checking if the user actually speaks the target language should be done before any other string is displayed to the user. */
	if(languageDetected && get_nsettings()->lang != lang::english)
	{
		ui::PopUp::pop_up(ui::Screen::bottom, ui::PopUp::ClaimScreen, [](ui::PopUp *pop_up) -> void {
			pop_up->make_builder<ui::Text>(str::language_detected)
				.size(0.42f)
				.wrap()
				.x(ui::layout::center_x)
				.add_to(pop_up);
			pop_up->fit_to_content();

			ui::Button *ok, *not_ok;

			/* user accepts detected language */
			pop_up->make_builder<ui::Button>(UI_GLYPH_A " OK")
				.when_clicked([pop_up](ui::Button *) -> bool {
					ui::RenderQueue::global()->render_and_then([pop_up]() -> void {
						pop_up->close();
					});
					return true;
				})
				.wrap()
				.under(pop_up->back())
				.disable_background()
				.size_children(0.36f)
				.add_to(&ok, pop_up);

			/* user does not accept detected language */
			pop_up->make_builder<ui::Button>(UI_GLYPH_B " Not OK")
				.when_clicked([pop_up](ui::Button *) -> bool {
					ui::RenderQueue::global()->render_and_then([pop_up]() -> void {
						pop_up->close(); /* close the popup so it's not shown on next global render */
						show_set_language(); /* select a new language */
					});
					return true;
				})
				.wrap().align_y(pop_up->back())
				.disable_background()
				.size_children(0.36f)
				.add_to(&not_ok, pop_up);

			/* TODO: This shouldn't have to be inlined, we should be able to use builder::next_center() */
			float w1 = ok->width();
			float w2 = not_ok->width();
			float total = w1 + w2 + 3.0f;

			float start = pop_up->width() / 2.0f - total / 2.0f;
			ok->set_x(start);
			not_ok->set_x(start + w1 + 3.0f);

			pop_up->fit_to_content();

			pop_up->make_builder<ui::ButtonCallback>(KEY_A | KEY_B)
				.when_kdown([pop_up](u32 keys) -> bool {
					ui::RenderQueue::global()->render_and_then([pop_up, keys]() -> void {
						pop_up->close();
						if(keys & KEY_B)
							show_set_language();
					});
					return true;
				})
				.add_to(pop_up);
		});
	}

	/*
		using a dev unit / enabling Luma3DS's "Enable dev UNITINFO" setting can cause unexpected behavior.
		this is because dev units use different crypto keys for content, and thus installing retail content
		on these systems can trigger hash check errors.
	*/
	if(!(ENVINFO & 1))
	{
		flog("Detected dev ENVINFO, aborting startup");

		ui::I18NEnabledRenderQueue queue;

		ui::builder<ui::Text>(ui::Screen::top, str::dev_unitinfo)
			.x(ui::layout::center_x).y(45.0f)
			.wrap()
			.add_to(queue);

		queue.render_finite_button(KEY_START | KEY_B);
		exit(0);
	}

#if VERSION_CHECK
	// Check if we are under system version 9.6 (9.6 added seed support, which is essential for most new titles)
	OS_VersionBin version, ignore;
	if(R_SUCCEEDED(osGetSystemVersionData(&ignore, &version)))
		/* it seems devkits are cursed, they all have versions < 1.0.0, so we'll allow that specific intel lists 0.24.38 as the
		 * first devkit with seed support but i haven't tested so i'll keep it like this for now. From these two sources
		 *  https://gbatemp.net/attachments/complete_ctr_systemupdater_list-png.391094/
		 *  https://en-americas-support.nintendo.com/app/answers/detail/a_id/667/~/nintendo-3ds-system-update-history */
		if( SYSTEM_VERSION(version.mainver, version.minor, version.build) > SYSTEM_VERSION(1,0,0)
		 && SYSTEM_VERSION(version.mainver, version.minor, version.build) < SYSTEM_VERSION(9,6,0))
		{
			flog("User is on an unsupported system version: %d.%d.%d", version.mainver, version.minor, version.build);
			ui::notice(str::outdated_system);
			exit(0);
		}
#endif

	/* (hopefully) prevent some users from losing money */
	{
		ui::I18NEnabledRenderQueue rq;
		
		ui::builder<ui::Text>(ui::Screen::top, str::startup_notice_title)
			.size(0.85f)
			.x(ui::layout::center_x)
			.y(5.0f)
			.add_to(rq);
		
		ui::builder<ui::Text>(ui::Screen::top, str::startup_notice_text)
			.size(0.48f)
			.max_width(376.0f)
			.x(ui::layout::center_x)
			.under(rq.back())
			.wrap()
			.add_to(rq);
		
		ui::builder<ui::Text>(ui::Screen::top, "github.com/p0mpurin/just-an-hshop-fork")
			.size(0.44f)
			.max_width(376.0f)
			.x(ui::layout::center_x)
			.under(rq.back(), 5.0f)
			.wrap()
			.add_to(rq);
		
		ui::builder<ui::Text>(ui::Screen::top, str::press_any_button_continue)
			.size(0.46f)
			.x(ui::layout::center_x)
			.y(ui::layout::bottom)
			.add_to(rq);
		
		/* broken cpad/cstick would cause this to dismiss immediately */
		const u32 btn_exclude_mask = KEY_CSTICK_LEFT | KEY_CSTICK_RIGHT | KEY_CSTICK_UP | KEY_CSTICK_DOWN | 
									 KEY_CPAD_LEFT | KEY_CPAD_RIGHT | KEY_CPAD_UP | KEY_CPAD_DOWN;
		rq.render_finite_button(0xFFFFFFFF & ~(btn_exclude_mask));
	}

	if(should_show_background_tip())
	{
		mkdir("/3ds", 0777);
		mkdir("/3ds/3hs", 0777);
		mkdir("/3ds/3hs/backgrounds", 0777);
		ui::notice(str::background_first_launch, 42.0f);
		mark_background_tip_seen();
	}

	/* bottom screen button row setup */

	ui::builder<ui::Text>(ui::Screen::top) /* text is not immediately set */
		.x(ui::layout::center_x)
		.y(4.0f)
		.tag(ui::tag::action)
		.wrap()
		.add_to(ui::RenderQueue::global());

	/* buttons */
	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::settings_image)
		.when_clicked([](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then(show_settings);
			return true;
		})
		.disable_background()
		.wrap()
		.x(5.0f)
		.y(210.0f)
		.tag(ui::tag::settings)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::more_image)
		.when_clicked([](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then(show_more);
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::more)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::search_image)
		.when_clicked([](ui::Button *btn) -> bool {
			ui::RenderQueue::global()->render_and_then([btn]() -> void {
				btn->sub_widget<ui::Sprite>()->set_opacity(0.5f);
				btn->set_enabled(false);
				show_search();
				btn->sub_widget<ui::Sprite>()->set_opacity(1.0f);
				btn->set_enabled(true);
			});
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::search)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::Button>(ui::Screen::bottom, ui::Sprite::theme, ui::theme::random_image)
		.when_clicked([](ui::Button *btn) -> bool {
			ui::RenderQueue::global()->render_and_then([btn]() -> void {
				btn->sub_widget<ui::Sprite>()->set_opacity(0.5f);
				btn->set_enabled(false);
				hsapi::Title t;
				if(R_SUCCEEDED(hsapi::call(hsapi::random, t)) && show_extmeta(t))
					install::gui::hs_cia(t);
				btn->sub_widget<ui::Sprite>()->set_opacity(1.0f);
				btn->set_enabled(true);
			});
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::random)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::Button>(ui::Screen::bottom, str::queue)
		.additional_i18n_update([](ui::Button *btn, lang::type) -> void {
			btn->set_x(ui::right(ui::RenderQueue::global()->find_tag(ui::tag::random), btn));
		})
		.when_clicked([](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then(show_queue);
			return true;
		})
		.disable_background()
		.wrap()
		.right(ui::RenderQueue::global()->back())
		.y(210.0f)
		.tag(ui::tag::queue)
		.add_to(ui::RenderQueue::global());

	/* konami */

	ui::builder<ui::KonamiListner>(ui::Screen::top)
		.tag(ui::tag::konami)
		.add_to(ui::RenderQueue::global());

	/* indicators */

	ui::builder<ui::FreeSpaceIndicator>(ui::Screen::top)
		.tag(ui::tag::free_indicator)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::StatusLine>(ui::Screen::top)
		.tag(ui::tag::status)
		.add_to(ui::RenderQueue::global());

#if TIP_GIVER
	ui::builder<TipGiver>(ui::Screen::top)
		.add_to(ui::RenderQueue::global());
#endif

	ui::builder<ui::TimeIndicator>(ui::Screen::top)
		.add_to(ui::RenderQueue::global());

	ui::builder<ui::BatteryIndicator>(ui::Screen::top)
		.add_to(ui::RenderQueue::global());

#ifndef RELEASE
	ui::builder<FrameCounter>(ui::Screen::top)
		.size(0.4f)
		.x(ui::layout::right).y(20.0f)
		.add_to(ui::RenderQueue::global());
#endif

	ui::builder<ui::NetIndicator>(ui::Screen::top)
		.tag(ui::tag::net_indicator)
		.add_to(ui::RenderQueue::global());


#ifdef DEVICE_ID
	u32 devid = 0;
	panic_assert(R_SUCCEEDED(psInit()), "failed to initialize PS");
	PS_GetDeviceId(&devid);
	psExit();
	// DRM Check failed
	if(devid != DEVICE_ID)
	{
		flog("Piracyception");
		(* (int *) nullptr) = 0xdeadbeef;
	}
#endif

	{
		Result dspres = 0;

		ilog("Ensuring DSP firm is dumped...");
		panic_assert(R_SUCCEEDED(dspres = ctr::mng::dspfirm::ensure_auto()), "failed to ensure dsp firm existence");
		ilog("DSP firm dump result: %s", dspres ? "skipped - found existing dspfirm" : "dumped successfully");
	}

	/* init audio subsystem */

	{
		ilog("Initializing audio system");
		panic_assert(R_SUCCEEDED(player_init()), "failed to initialize audio system");
		atexit(player_exit);

		ilog("Loading audio configuration");
		panic_assert(acfg_load() == ACE_NONE, "failed to load audio configuration");
		atexit(acfg_free);

		player_set_switch_callback([](const struct cwav *cwav) -> void {
			if(cwav->artist) ui::set_status(PSTRING(playing_x_by_y, cwav->title, cwav->artist));
			else             ui::set_status(PSTRING(playing_x, cwav->title));
		});

		panic_assert(acfg_realise() == ACE_NONE, "failed to set audio configuration");

		ui::set_select_command_handler([](u32 kDown) -> void {
			/* process audio command */
			if(kDown & KEY_L) player_previous();
			if(kDown & KEY_R) player_next();
			if(kDown & KEY_A) player_unpause();
			if(kDown & KEY_B) player_pause();
			if(kDown & KEY_X) { player_halt(); ui::reset_status(); }
		});
	}

	/*
		ask AC to connect to wifi, which should hopefully avoid any wrong "network unavailable" errors
		this is supposed to be done, otherwise AC does not consider you as wanting to use the network
	*/

	atexit(wlan_thread_exit);
	panic_assert(R_SUCCEEDED(svcCreateEvent(&wlan_disconnect_event, RESET_ONESHOT)), "Failed creating WLAN disconnect event");
	panic_assert(R_SUCCEEDED(svcCreateMutex(ctr::wlan::connect_mtx(), false)), "Failed creating WLAN connect mutex");

	if (!disableAutoWlan)
	{
		enum { check_net = 0, check_inet, done, error }
		wlan_checkstate = check_net;

		static constexpr const str::type msgs[2] =
		{
			str::check_net,
			str::check_inet
		};


		do {
			ui::loading([&wlan_checkstate, &res]() -> void {
				switch(wlan_checkstate)
				{
				case check_net:
					{
						if (!ctr::wlan::is_enabled())
						{
							ilog("WiFi is disabled - enabling");
							panic_assert(R_SUCCEEDED(ctr::wlan::enable()), "Failed enabling WiFi");
							ilog("Enabled WiFi");
						}
						wlan_checkstate = check_inet;
					}
					break;
				case check_inet:
					{
						ilog("Connecting to the internet");

						if(R_FAILED(res = ctr::wlan::connect(wlan_disconnect_event))) {
							elog("Failed connecting to the internet!");
							wlan_checkstate = error;
						}
						else {
							ilog("Connected successfully");
							wlan_checkstate = done;
						}
					}
					break;
				default:
					break;
				}
			}, msgs[wlan_checkstate]);
		} while (wlan_checkstate != done && wlan_checkstate != error);

		if (wlan_checkstate == error) {
			error_container conn_err = get_error(res);
			report_error(conn_err, "Internet connection");
			handle_error(conn_err);
		}

		panic_assert(R_SUCCEEDED(svcCreateEvent(&wlan_thread_exit_event, RESET_ONESHOT)), "Failed creating WLAN thread exit event");

		wlan_thread = new ctr::thread<Handle&, Handle&>([](Handle exit_evt, Handle disconnect_evt) -> void {
			Handle handles[2] = { exit_evt, disconnect_evt };
			bool reconnect_mode = true; /* in case we start disconnected */
			while (1) {
				if (reconnect_mode) {
					ilog("[auto-wlan] attempting to reconnect");

					if (!ctr::wlan::is_enabled()) {
						ilog("[auto-wlan] cannot reconnect, wlan is disabled");
					}
					else {
						if (!ctr::wlan::is_connected()) {
							ilog("[auto-wlan] connecting");
							Result wlan_conn_res = ctr::wlan::connect(handles[1], 10000000000LLU);
							ilog("[auto-wlan] connect result: %08lX", wlan_conn_res);
							if (R_SUCCEEDED(wlan_conn_res)) {
								reconnect_mode = false;
								ilog("[auto-wlan] reconnected");
								continue;
							}
						}
						else {
							ilog("[auto-wlan] exiting reconnect mode, already connected");
							reconnect_mode = false;
							continue;
						}
					}
					ilog("[auto-wlan] attempting reconnect in 5s");
					svcSleepThread(5000000000LL);
					continue;
				}

				s32 idx = -1;
				Result wait_res = svcWaitSynchronizationN(&idx, handles, 2, false, -1);

				if (R_FAILED(wait_res)) /* we should not be closing any of these handles before exiting the thread */
					panic(wait_res);

				if (idx == 0) { /* exit event */
					ilog("[auto-wlan] exit");
					return;
				}
				else if (idx == 1) { /* disconnect event */
					ilog("[auto-wlan] detected disconnect, entering reconnect mode");
					reconnect_mode = true;
					continue;
				}
				else {
					panic("invalid waitsync index");
				}
			}
		}, 2, wlan_thread_exit_event, wlan_disconnect_event);
	}

#if defined(RELEASE) && !defined(PRERELEASE)
	ui::set_focus(true);
	// Check if luma is installed
	// 1. Citra is used; not compatible
	// 2. Other cfw used; not supported
	if(!isLuma)
	{
		flog("Luma3DS is not installed, user is using an unsupported CFW or running in Citra");
		block_app(str::luma_not_installed, str::install_luma);
	}

	ilog("Checking for updates");
	Result update_res = 0xE7E3FFFF; /* invalid result value */
	update::update_status us = update::update_app(update_res);

	switch (us) {
		case update::update_status::failed_update_check:
			ilog("Update check failed (%08lX), continuing custom build", update_res);
			if(http::http_last_error()[0])
				ilog("Nocturne updater detail: %s", http::http_last_error());
			break;
		case update::update_status::failed_update_install:
			ilog("Update install failed, app blocked");
			block_app(str::update_install_failed, str::update_try_again_or_update_manually, update_res);
			break;
		case update::update_status::up_to_date:
			ilog("up-to-date");
			break;
		case update::update_status::updated_successfully:
			ilog("Successfully updated from " VERSION);
			exit(0);
			break;
		case update::update_status::upstream_update_available:
			ilog("New official 3hs source is available; Nocturne merge required");
			ui::notice("A newer official 3hs release is available.\n\nNocturne will keep running so your UI and performance changes are preserved. The upstream source update must be merged into a new Nocturne build; installing the stock CIA would replace this fork.", 42.0f);
			break;
		case update::update_status::update_required:
		case update::update_status::outdated_hb_release:
			ilog("Upstream update state detected; continuing fork build");
			break;
	}

	ui::set_focus(false);
#endif

	while(R_FAILED(hsapi::call(hsapi::fetch_index)))
		show_more();

	vlog("Done fetching index.");

	/* setup finished */

	size_t catptr = 0, subptr = 0;
	hsapi::hcid associatedcat = -1, associatedsub = -1;
	std::vector<hsapi::PartialTitle> titles;
	bool visited_sub = false, visited_title = false;
	next::title_reenter_data grdata;

	// Old logic was cursed, made it a bit better :blobaww:
	while(aptMainLoop())
	{
sel_cat:
		hsapi::hcid cat = next::sel_cat(&catptr);
		// User wants to exit app
		if(cat == next_cat_exit) break;
		ilog("NEXT(c): %s", hsapi::category(cat).name.c_str());
		/* we need to reset this since we've changed categories, meaning
		 * the subcategory data is invalid */
		if(cat != associatedcat)
		{
			associatedsub = -1;
			subptr = 0;
		}
		associatedcat = cat;

sel_subcat:
		hsapi::hcid sub = next::sel_sub(cat, &subptr);
		if(sub == next_sub_back) goto sel_cat;
		if(sub == next_sub_exit) break;
		ilog("NEXT(s): %s", hsapi::subcategory(cat, sub).name.c_str());

		if(associatedsub != sub)
		{
			titles.clear();
			const hsapi::Category& cato = hsapi::category(cat);
			const hsapi::Subcategory& subo = hsapi::subcategory(cat, sub);
			visited_title = false;
			if(R_FAILED(hsapi::call(hsapi::titles_in, titles, cato, subo)))
				goto sel_subcat;
		}
		else visited_title = true;
		associatedsub = sub;

sel_title:
//		hsapi::hid id = next::sel_icon_title(titles, hsapi::category(cat), hsapi::subcategory(cat, sub)); //&grdata, visited_title);
		hsapi::hid id = next::sel_title(titles, &grdata, visited_title);
		if(id == next_title_back) goto sel_subcat;
		if(id == next_title_exit) break;
		/* this means we've been in the category before,
		 * this will get set to false if we re-enter */
		visited_title = true;

		ilog("NEXT(g): %u", id);

		hsapi::Title meta;
		if(show_extmeta_lazy(titles, id, &meta))
			install::gui::hs_cia(meta);

		goto sel_title;
	}

	ilog("Goodbye, app deinit");
	ilog("Disconnecting from WiFi");

	wlan_thread_exit();

	res = ctr::wlan::disconnect();
	if (R_FAILED(res)) {
		ilog("Failed disconnecting from WiFi");
	} else {
		ilog("Disconnected from WiFi");
	}

	exit(0);
}
