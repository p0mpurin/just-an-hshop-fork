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

#include "panic.hh"

#include <stdlib.h>
#include <malloc.h>

#include <ui/base.hh>

#include "audio/player.h"
#include "dmn.hh"
#include "httpclient.hh"
#include "install.hh"
#include "i18n.hh"
#include "log.hh"

#define SOC_ALIGN       0x100000
#define SOC_BUFFERSIZE  0x100000

static bool gfx_enabled = false;
void panic_enable_gfx() { gfx_enabled = true; }

static u32 *g_socbuf = nullptr;

Result init_services(bool& isLuma)
{
	Result res;

	Handle lumaCheck;
	isLuma = R_SUCCEEDED(svcConnectToPort(&lumaCheck, "hb:ldr"));
	if(isLuma) svcCloseHandle(lumaCheck);

#define TRYINIT(service_pretty, func, ...) if(R_FAILED(res = (func))) do { elog("Failed to initialize " service_pretty ", %08lX", res); return res; } while(0)
	// Doesn't work in citra
	if(isLuma) TRYINIT("MCU::HWC", mcuHwcInit());
	TRYINIT("ptm:sysm", ptmSysmInit());
	TRYINIT("nwm::EXT", nwmExtInit());
	TRYINIT("romfs", romfsInit());
	TRYINIT("ptm:u", ptmuInit());
	TRYINIT("cfg:u", cfguInit());
	TRYINIT("ndm:u", ndmuInit());
	TRYINIT("apt", aptInit());
	TRYINIT("fs", fsInit());
	TRYINIT("am", amInit());
	TRYINIT("ac", acInit());
#undef TRYINIT

	if(!(g_socbuf = (u32 *) memalign(SOC_ALIGN, SOC_BUFFERSIZE)))
	{
		elog("failed to allocate buffer of %X (aligned %X) for SOC", SOC_BUFFERSIZE, SOC_ALIGN);
		return res;
	}
	if(R_FAILED((res = socInit(g_socbuf, SOC_BUFFERSIZE))))
	{
		free(g_socbuf);
		g_socbuf = nullptr;
		elog("failed to initialize SOC: %08lX", res);
		return res;
	}

	if(R_FAILED(res = http::ResumableDownload::global_init()))
	{
		free(g_socbuf);
		g_socbuf = nullptr;
		elog("Failed to initialize ResumableDownload: %08lX", res);
		return res;
	}

	return res;
}

void exit_services()
{
	/* not really appropriate here, but whatever */
	ctr::dmn::delete_sleep_lock();
	http::ResumableDownload::global_abort();
	install::global_abort();

	if(g_socbuf != nullptr)
	{
		socExit();
		free(g_socbuf);
		g_socbuf = nullptr;
	}

	mcuHwcExit();
	ptmSysmExit();
	nwmExtExit();
	romfsExit();
	ptmuExit();
	cfguExit();
	ndmuExit();
	aptExit();
	fsExit();
	amExit();
	acExit();
}

static void pusha(ui::I18NEnabledRenderQueue& queue)
{
	ui::builder<ui::Text>(ui::Screen::top, str::press_a_exit)
		.x(ui::layout::center_x)
		.y(ui::layout::bottom)
		.wrap()
		.add_to(queue);
}

static void pusherror(const error_container& err, ui::I18NEnabledRenderQueue& queue, float base)
{
	ui::Text *first;
	ui::BaseWidget *back = queue.back();
	ui::builder<ui::Text>(ui::Screen::top, format_err(err.sDesc, err.iDesc))
		.x(ui::layout::center_x)
		.y(base)
		.wrap()
		.add_to(&first, queue);
	if(base == 0.0f) first->set_y(ui::under(back, first));
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(result_code, "0x" + pad8code(err.full)))
		.manual_i18n_update([&err](ui::Text *t, lang::type) -> void {
			t->set_text(PSTRING(result_code, "0x" + pad8code(err.full)));
		})
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(level, format_err(err.sLvl, err.iLvl)))
		.manual_i18n_update([&err](ui::Text *t, lang::type) -> void {
			t->set_text(PSTRING(level, format_err(err.sLvl, err.iLvl)));
		})
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(summary, format_err(err.sSum, err.iSum)))
		.manual_i18n_update([&err](ui::Text *t, lang::type) -> void {
			t->set_text(PSTRING(summary, format_err(err.sSum, err.iSum)));
		})
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(module, format_err(err.sMod, err.iMod)))
		.manual_i18n_update([&err](ui::Text *t, lang::type) -> void {
			t->set_text(PSTRING(module, format_err(err.sMod, err.iMod)));
		})
		.x(ui::layout::center_x)
		.under(queue.back())
		.add_to(queue);
}

void handle_error(const error_container& err, const std::string *label)
{
	ui::I18NEnabledRenderQueue queue;
	pusha(queue);
	float base = 50.0f;
	if(label)
	{
		ui::builder<ui::Text>(ui::Screen::top, *label)
			.x(ui::layout::center_x)
			.y(30.0f)
			.wrap()
			.add_to(queue);
		base = 0.0f;
	}
	pusherror(err, queue, base);
	queue.render_finite_button(KEY_A | KEY_B);
}


[[noreturn]] static void panic_core(const std::string& caller, ui::I18NEnabledRenderQueue& queue)
{
	elog("PANIC -- THERE IS AN ERROR IN THE APPLCIATION");
	elog("Caller is %s", caller.c_str());
	aptSetHomeAllowed(true); /* these might be set otherwise in other parts of the code */
	C3D_FrameRate(60.0f);

	ui::Text *action = ui::RenderQueue::global()->find_tag<ui::Text>(ui::tag::action);
	/* panic may be called before core ui is set-up so we can't be sure
	 * ui::tag::action is already active */
	if(action == nullptr)
	{
		ui::builder<ui::Text>(ui::Screen::top, str::fatal_panic)
			.x(ui::layout::center_x)
			.y(4.0f)
			.wrap()
			.add_to(&action, ui::RenderQueue::global());
	}

	else
		/* this will be the final focus shift so we don't need to revert the state after */
		ui::set_desc(str::fatal_panic);
	ui::set_focus(true);

	/* disable the select command handler */
	ui::set_select_command_handler([](u32) -> void { });
	/* stop audio */
	player_exit();

	pusha(queue);
	ui::builder<ui::Text>(ui::Screen::top, caller)
		.x(ui::layout::center_x)
		.under(action)
		.wrap()
		.add_to(queue);

	ui::RenderQueue::terminate_render();
	queue.render_finite_button(KEY_A | KEY_B);

	exit(0);
}

[[noreturn]] static void panic_preinit_impl(const std::string& caller, const std::string& msg)
{
	elog("PANIC (PREINIT) -- THERE IS AN ERROR IN THE APPLCIATION");
	elog("Caller is %s", caller.c_str());

	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);

	printf("\x1b[31mFATAL PANIC -- INITIALIZATION FAILED\x1b[0m\n");
	printf("Failed to initialize 3hs: %s\n", msg.c_str());
	printf("  from %s.\n", caller.c_str());
	printf("\x1b[31mFATAL PANIC -- INITIALIZATION FAILED\x1b[0m\n");
	printf("Press [A] to exit\n");

	while(aptMainLoop())
	{
		hidScanInput();
		if(hidKeysDown() & (KEY_A | KEY_B))
			break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	exit(0);
}

[[noreturn]] void panic(const std::string& msg, const SourceLocation& caller)
{
	elog("PANIC MESSAGE -- %s", msg.c_str());
	if(!gfx_enabled) panic_preinit_impl(caller.to_string(), msg);
	ui::I18NEnabledRenderQueue queue;

	ui::builder<ui::Text>(ui::Screen::top, msg)
		.x(ui::layout::center_x)
		.y(70.0f)
		.wrap()
		.add_to(queue);

	panic_core(caller.to_string(), queue);
}

[[noreturn]] void panic(Result res, const SourceLocation& caller)
{
	elog("PANIC RESULT -- 0x%08lX", res);
	if(!gfx_enabled)
		panic_preinit_impl(caller.to_string(), "Failed, result: " + pad8code(res) + "\n" + get_error(res).sDesc);
	ui::I18NEnabledRenderQueue queue;

	error_container err = get_error(res);
	pusherror(err, queue, 70.0f);

	panic_core(caller.to_string(), queue);
}

[[noreturn]] void panic(const SourceLocation& caller)
{
	if(!gfx_enabled) panic_preinit_impl(caller.to_string(), "fatal panic");
	ui::I18NEnabledRenderQueue queue;
	panic_core(caller.to_string(), queue);
}

std::string SourceLocation::to_string() const
{
	if(this->filename) return std::string(this->function) + "@" + std::string(strrchr(this->filename, '/') + 1) + ":" + std::to_string(this->lineno);
	else if(this->function) return std::string(this->function) + ":" + std::to_string(this->lineno);
	else return "(null location)";
}

void SourceLocation::log(const char *msg) const
{
	if(this->function) dlog("%s: %s", this->to_string().c_str(), msg);
}
