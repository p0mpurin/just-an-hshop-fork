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

#include <ui/base.hh>
#include <panic.hh>

#include "sync.hh"
#include "widgets/status_line.hh"
#include "image_ldr.hh"
#include "settings.hh"
#include "i18n.hh"
#include "log.hh"
#include "mng.hh"

#include <3rd/stb_image.h>
#include <vector>
#include <cmath>

/* internal constants */
#define THEME_PATH get_settings()->isLightMode ? "romfs:/light.hstx" : "romfs:/dark.hstx"
#define SPRITESHEET_PATH "romfs:/gfx/next.t3x"
#define TOP_WIDE_TRANSFER_FLAGS \
	(GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) | \
	GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
	GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

/* global variables */
static C3D_RenderTarget *g_top;
static C3D_RenderTarget *g_bot;
static bool g_top_wide;

static ui::RenderQueue g_renderqueue;
static std::list<ui::RenderQueue *> g_i18n_rq_register;
static ui::SpriteStore g_spritestore;

/* used for debugging purposes */
static bool g_inRender = false;

static ui::SlotManager slotmgr { nullptr };
static ui::ScopedWidget<ui::Sprite> top_background;
static ui::ScopedWidget<ui::Sprite> bottom_background;
static C2D_Image user_top_background = { nullptr, nullptr };
static C2D_Image user_bottom_background = { nullptr, nullptr };
static bool user_background_active = false;
static float user_top_bg_width = 400.0f; /* actual rendered width of the top wallpaper */

static ui::select_command_handler select_command;
static bool previous_select_was_command;
static u32 my_kheld, my_kdown;

static bool touch_locked = false;

static LightLock render_and_then_lock;
static LightLock before_render_lock;
static LightLock in_render_lock;

enum LEDFlags_V {
	LED_NONE          = 0,
	LED_RESET_SLEEP   = 1,
	LED_TIMEOUT       = 2,
};
static u8 LEDFlags = LED_NONE;
static time_t LEDExpireTime;

UI_CTHEME_GETTER(color_toggle_green, ui::theme::toggle_green_color)
UI_CTHEME_GETTER(color_toggle_red, ui::theme::toggle_red_color)
UI_CTHEME_GETTER(color_toggle_slider, ui::theme::toggle_slider_color)
UI_CTHEME_GETTER(color_button_border, ui::theme::button_border_color)
UI_CTHEME_GETTER(color_button, ui::theme::button_background_color) UI_CTHEME_GETTER(color_bg, ui::theme::background_color)
UI_CTHEME_GETTER(color_text, ui::theme::text_color)

/* helpers */

static C3D_RenderTarget *create_top_target()
{
	g_top_wide = !!ISET_TOP_WIDE_EXPERIMENTAL;

	/* 800px wide mode requires New 3DS hardware — the Old 2DS/3DS LCD
	 * controller does not support the wider framebuffer stride. */
	if(g_top_wide && !ctr::mng::is_n3ds())
	{
		ilog("800px mode disabled: Old 3DS/2DS hardware does not support wide framebuffer");
		g_top_wide = false;
	}

	gfxSet3D(false);
	gfxSetWide(g_top_wide);

	if(g_top_wide)
	{
		C3D_RenderTarget *target = C3D_RenderTargetCreate(240, 800,
			GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
		if(target)
		{
			C3D_RenderTargetSetOutput(target, GFX_TOP, GFX_LEFT,
				TOP_WIDE_TRANSFER_FLAGS);
			ilog("800px top-screen mode active");
			return target;
		}

		elog("800px mode: C3D_RenderTargetCreate failed, falling back to 400px");
		g_top_wide = false;
		gfxSetWide(false);
	}

	return C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
}

/* Draw the top-screen background respecting the current display mode.
 * In 800px wide mode, user wallpapers are already pre-cropped to 800px,
 * and theme background sprites (always 400px) are centered. */
static void draw_top_background(ui::Keys& keys)
{
	if(user_background_active)
	{
		/* Center the wallpaper in 800px mode if it's narrower than the screen.
		 * This avoids ugly upscaling of 400px wallpapers to 800px. */
		float bg_x = g_top_wide
			? (800.0f - user_top_bg_width) * 0.5f
			: 0.0f;
		C2D_DrawImageAt(user_top_background, bg_x, 0.0f, -0.9f);
	}
	else if(top_background->has_image())
	{
		if(g_top_wide)
		{
			/* Center the 400px theme sprite in the 800px framebuffer */
			top_background->set_x(200.0f);
		}
		else
		{
			top_background->set_x(0.0f);
		}
		top_background->render(keys);
	}
}

static inline void begin_top_scene()
{
	C2D_SceneBegin(g_top);
	C2D_ViewReset();
}

static inline void begin_bottom_scene()
{
	C2D_SceneBegin(g_bot);
	C2D_ViewReset();
}

Result ui::shell_is_open(bool *is_open)
{
	Result res;
	u8 state;
	if(R_FAILED(res = PTMU_GetShellState(&state)))
	{
		elog("PTMU_GetShellState() failed: %08lX", res);
		state = 0; /* just assume it's closed... */
	}
	*is_open = state == 1;
	return res;
}

static inline float center_pos(float max, float width)
{
	return (max / 2.0f) - (width / 2.0f);
}

float ui::ycenter_rel(ui::BaseWidget *wid, ui::BaseWidget *owid)
{
	/* this probably doesn't work for all widgets, oh well, it works for ui::CheckBox, it's purpose */
	return wid->get_y() + center_pos(wid->height(), owid->height());
}

float ui::transform(ui::BaseWidget *wid, float v)
{
	if(v >= 0.0f) return v;
	switch((int) v)
	{
	case (int) ui::layout::bottom:
		return ui::dimensions::height - wid->height() - 10.0f; /* padding of 10.0f */
	case (int) ui::layout::right:
		return ui::screen_width(wid->renders_on()) - wid->width() - 10.0f; /* padding of 10.0f */
	case (int) ui::layout::center_x:
		return center_pos(ui::screen_width(wid->renders_on()), wid->width());
	case (int) ui::layout::center_y:
		return center_pos(ui::dimensions::height, wid->height());
	}

	/* v <= 0 is invalid */
	return 0.0f;
}

float ui::under(ui::BaseWidget *from, ui::BaseWidget *newel, float pad)
{
	((void) newel); /* newel is here for consistency */
	return from->get_y() + from->height() + pad;
}

float ui::above(ui::BaseWidget *from, ui::BaseWidget *newel, float pad)
{
	return from->get_y() - newel->height() - pad;
}

float ui::right(ui::BaseWidget *from, ui::BaseWidget *newel, float pad)
{
	((void) newel); /* newel is here for consistency */
	return from->get_x() + from->width() + pad;
}

float ui::left(ui::BaseWidget *from, ui::BaseWidget *newel, float pad)
{
	return from->get_x() - newel->width() - pad;
}

void ui::set_double_center(BaseWidget *first, BaseWidget *second, float pad)
{
	float w1 = first->width();
	float w2 = second->width();
	float total = w1 + w2 + pad;

	float start = center_pos(ui::screen_width(first->renders_on()), total);
	first->set_x(start);
	second->set_x(start + w1 + pad);
}

float ui::center_align_y(BaseWidget *from, BaseWidget *newel)
{
	return center_pos(from->height(), newel->height()) + from->get_y();
}

#define NOTICE_BUILDER(msg,rq) \
	ui::builder<ui::Text>(ui::Screen::top, str::press_a_exit) \
		.size(0.48f) \
		.max_width(380.0f) \
		.x(ui::layout::center_x) \
		.y(ui::layout::bottom) \
		.wrap() \
		.add_to(rq); \
	ui::builder<ui::Text>(ui::Screen::top, msg) \
		.size(0.45f) \
		.max_width(370.0f) \
		.x(ui::layout::center_x) \
		.y(ypos) \
		.wrap() \
		.add_to(rq);

void ui::notice(const std::string& msg, float ypos)
{
	ui::RenderQueue queue;

	NOTICE_BUILDER(msg,queue)

	queue.render_finite_button(KEY_A | KEY_START | KEY_B);
}

void ui::notice(str::type msgid, float ypos)
{
	ui::I18NEnabledRenderQueue queue;

	NOTICE_BUILDER(msgid, queue)

	queue.render_finite_button(KEY_A | KEY_START | KEY_B);
}

#undef NOTICE_BUILDER

static ui::slot_color_getter slotmgr_getters[] = {
	color_bg
};

/* both from supplement_sysfont_merger.c */
extern "C" Result font_merger_run(void);
extern "C" void font_merger_destroy(void);

static void common_init()
{
	g_spritestore.open(SPRITESHEET_PATH);
	/* This is always the correct theme: before calling ui::init() load_current_theme() is called
	 * so that the selected theme is in the back of the themes() array */
	ui::Theme::global()->replace_with(themes().back());
	ui::ThemeManager::global()->reget();
	top_background.setup(ui::Screen::top, ui::Sprite::theme, ui::theme::background_top_image);
	top_background->set_x(0.0f);
	top_background->set_y(0.0f);
	bottom_background.setup(ui::Screen::bottom, ui::Sprite::theme, ui::theme::background_bottom_image);
	bottom_background->set_x(0.0f);
	bottom_background->set_y(0.0f);
	if(!get_nsettings()->background_path.empty())
		ui::set_user_background(get_nsettings()->background_path);
	slotmgr = ui::ThemeManager::global()->get_slots(nullptr, "__global_slot_manager", 1, slotmgr_getters);
	panic_if_err_3ds(font_merger_run());
	LightLock_Init(&before_render_lock);
	LightLock_Init(&render_and_then_lock);
	LightLock_Init(&in_render_lock);

	ui::builder<ui::detail::BackgroundObscurer>(ui::Screen::bottom).hide().tag(ui::tag::obscure_bottom).add_to(g_renderqueue);
	ui::builder<ui::detail::BackgroundObscurer>(ui::Screen::top).hide().tag(ui::tag::obscure_top).add_to(g_renderqueue);
}

void ui::init(C3D_RenderTarget *top, C3D_RenderTarget *bot)
{
	g_top = top;
	g_bot = bot;
	g_top_wide = false;
	common_init();
}

bool ui::init()
{
	gfxInitDefault();
	if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) return false;
	if(!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) return false;
	C2D_Prepare();

	g_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	g_top = create_top_target();
	if(!g_top) return false;

	common_init();
	return true;
}

void ui::exit()
{
	if(user_background_active)
	{
		delete_image(user_top_background);
		delete_image(user_bottom_background);
		user_background_active = false;
	}
	font_merger_destroy();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
}

void ui::set_ticker(const std::string& text)
{
	ui::StatusLine *sl = ui::RenderQueue::global()->find_tag<ui::StatusLine>(ui::tag::status);
	panic_assert(sl, "status line not set up");
	sl->reset();
	sl->ticker(text);
}

void ui::set_status(const std::string& text)
{
	ui::StatusLine *sl = ui::RenderQueue::global()->find_tag<ui::StatusLine>(ui::tag::status);
	panic_assert(sl, "status line not set up");
	sl->reset();
	sl->run(text);
}

bool ui::status_running()
{
	ui::StatusLine *sl = ui::RenderQueue::global()->find_tag<ui::StatusLine>(ui::tag::status);
	return sl->is_running();
}

void ui::reset_status()
{
	ui::StatusLine *sl = ui::RenderQueue::global()->find_tag<ui::StatusLine>(ui::tag::status);
	panic_assert(sl, "status line not set up");
	sl->reset();
}

bool ui::set_focus(bool focus, bool with_action)
{
	/* panic() called before core global widgets are setup */
	if(!ui::RenderQueue::global()->find_tag(ui::tag::action))
		return false;
	bool ret = ui::RenderQueue::global()->find_tag(ui::tag::settings)->is_hidden();
	ui::RenderQueue::global()->find_tag(ui::tag::settings)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::search)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::konami)->set_hidden(focus);
	if (with_action) ui::RenderQueue::global()->find_tag(ui::tag::action)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::random)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::queue)->set_hidden(focus);
	ui::RenderQueue::global()->find_tag(ui::tag::more)->set_hidden(focus);
	return ret;
}

u8 ui::make_status_line_clear()
{
	u8 flags = 0;

	ui::BaseWidget *w = ui::RenderQueue::global()->find_tag(ui::tag::net_indicator);
	flags |= w->is_hidden() ? 0 : 1;
	w->set_hidden(true);

	w = ui::RenderQueue::global()->find_tag(ui::tag::free_indicator);
	flags |= w->is_hidden() ? 0 : 2;
	w->set_hidden(true);

	return flags;
}

void ui::restore_status_line(u8 flags)
{
	ui::RenderQueue::global()->find_tag(ui::tag::net_indicator)->set_hidden(!!(flags & 1));
	ui::RenderQueue::global()->find_tag(ui::tag::free_indicator)->set_hidden(!!(flags & 2));
}

ui::prev_desc ui::set_desc(const std::string& nlabel)
{
	ui::Text *action = ui::RenderQueue::global()->find_tag<ui::Text>(ui::tag::action);
	ui::prev_desc prev { .does_i18n = action->i18n_update_setup() };

	if (prev.does_i18n) {
		prev.desc.strid = action->get_i18n_strid();
		/* we're setting the string to one that isn't able to auto-i18n-update... */
		action->disable_i18n_update();
	}
	else
		prev.desc.str = strdup(action->get_text().c_str());

	action->set_text(nlabel);

	return prev;
}

ui::prev_desc ui::set_desc(str::type strid)
{
	ui::Text *action = ui::RenderQueue::global()->find_tag<ui::Text>(ui::tag::action);
	ui::prev_desc prev { .does_i18n = action->i18n_update_setup() };

	if (prev.does_i18n)
		prev.desc.strid = action->get_i18n_strid();
	else
		prev.desc.str = strdup(action->get_text().c_str());

	action->setup(strid);

	return prev;
}

ui::prev_desc ui::set_desc(ui::prev_desc& prev)
{
	return prev.does_i18n ?
		ui::set_desc(prev.desc.strid) :
		ui::set_desc(prev.desc.str);
}

/* class I18NEnabledRenderQueue */
ui::I18NEnabledRenderQueue::I18NEnabledRenderQueue()
{
	ui::RenderQueue::get_i18n_register().push_back(this);
}

ui::I18NEnabledRenderQueue::~I18NEnabledRenderQueue()
{
	ui::RenderQueue::get_i18n_register().remove(this);
}

/* class RenderQueue */

ui::RenderQueue::~RenderQueue()
{
	this->clear();
}

void ui::RenderQueue::clear()
{
	for(ui::BaseWidget *wid : this->bot) { wid->destroy(); delete wid; }
	for(ui::BaseWidget *wid : this->top) { wid->destroy(); delete wid; }

	this->signalBit = 0;

	this->backPtr = nullptr;
	this->top.clear();
	this->bot.clear();

	this->detach_after();
}

void ui::RenderQueue::push(ui::BaseWidget *wid)
{
	/* none is just an alias for bottom really */
	if(wid->renders_on() == ui::Screen::bottom || wid->renders_on() == ui::Screen::none)
		this->bot.push_back(wid);
	else if(wid->renders_on() == ui::Screen::top)
		this->top.push_back(wid);
	else
		panic("Trying to push widget that renders on an unknown screen");
	this->backPtr = wid;
	if(wid->processes_in_sleep())
		this->sleepProcessors.push_back(wid);
}

void ui::RenderQueue::remove(ui::BaseWidget *wid)
{
	if(wid->renders_on() == ui::Screen::bottom || wid->renders_on() == ui::Screen::none)
		this->bot.remove(wid);
	else if(wid->renders_on() == ui::Screen::top)
		this->top.remove(wid);
	else
		panic("Trying to remove widget that renders on an unknown screen");

	if(wid->processes_in_sleep())
		this->sleepProcessors.remove(wid);

	/* We can't forget to actually call the dtor or we'll leak memory */
	wid->destroy();
	delete wid;
}

void ui::set_select_command_handler(select_command_handler handler)
{
	select_command = handler;
}

void ui::scan_keys()
{
	hidScanInput();
	my_kdown = hidKeysDown();
	my_kheld = hidKeysHeld();
	if(select_command && hidKeysUp() & KEY_SELECT)
	{
		if(previous_select_was_command)
			previous_select_was_command = false;
		else
		{
			select_command(0);
			my_kheld = my_kdown = 0;
		}
	}
	else if((my_kheld & KEY_SELECT) && (my_kdown & ~KEY_SELECT))
	{
		previous_select_was_command = true;
		select_command(my_kdown);
		my_kheld = my_kdown = 0;
	}
	else if(my_kheld & KEY_SELECT)
	{
		my_kheld = my_kdown = 0;
	}
	if(touch_locked && (hidKeysUp() & KEY_TOUCH))
		touch_locked = false;
}

ui::Keys ui::RenderQueue::get_keys()
{
	ui::scan_keys();
	ui::Keys keys = {
		my_kdown, my_kheld, hidKeysUp(),
		{ 0, 0 }
	};
	if(!touch_locked)
		hidTouchRead(&keys.touch);
	return keys;
}

void ui::set_touch_lock(ui::Keys& keys)
{
	ui::set_touch_lock();
	memset(&keys.touch, 0, sizeof(keys.touch));
	keys.kDown &= ~KEY_TOUCH;
}

void ui::set_touch_lock()
{
	touch_locked = true;
}

u32 ui::kHeld() { return my_kheld; }
u32 ui::kDown() { return my_kdown; }

void ui::maybe_end_frame()
{
	ctr::LockedInScope l { &in_render_lock };
	if(g_inRender)
	{
		C3D_FrameEnd(0);
		g_inRender = false;
	}
}

void ui::background_rect(ui::Screen scr, float x, float y, float z, float w, float h)
{
	ui::ScopedWidget<ui::Sprite> *bg = scr == ui::Screen::top ? &top_background : &bottom_background;
	if(user_background_active)
	{
		/* Scrolling text uses this helper to erase its previous frame. An
		 * opaque black patch remains readable over arbitrary wallpapers. */
		C2D_DrawRectSolid(x, y, z, w, h, C2D_Color32(0, 0, 0, 232));
	}
	else if((*bg)->has_image())
	{
		/* cursed */
		C2D_Image img = *(*bg)->get_image();
		u16 wu = w, hu = h;
		Tex3DS_SubTexture subtex = {
			wu, hu,
			0, 0, 0, 0
		};
		img.subtex = &subtex;
		C2D_DrawImageAt(img, x, y, z);
	}
	else
	{
		/* slightly less cursed */
		C2D_DrawRectSolid(x, y, z, w, h, slotmgr.get(0));
	}
}

static bool make_cover_image(C2D_Image *out, const u8 *source, int sw, int sh, int dw, int dh)
{
	if(sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
		return false;

	std::vector<u32> pixels((size_t) dw * dh);
	const float scale = std::max(dw / (float) sw, dh / (float) sh);
	const float visible_w = dw / scale;
	const float visible_h = dh / scale;
	const float crop_x = (sw - visible_w) * 0.5f;
	const float crop_y = (sh - visible_h) * 0.5f;

	for(int y = 0; y < dh; ++y)
	{
		int sy = (int) (crop_y + (y + 0.5f) / scale);
		sy = std::max(0, std::min(sh - 1, sy));
		for(int x = 0; x < dw; ++x)
		{
			int sx = (int) (crop_x + (x + 0.5f) / scale);
			sx = std::max(0, std::min(sw - 1, sx));
			const u8 *rgba = source + ((size_t) sy * sw + sx) * 4;
			/* Same byte order produced by rgba_to_abgr(). */
			pixels[(size_t) y * dw + x] =
				((u32) rgba[0] << 24) | ((u32) rgba[1] << 16)
				| ((u32) rgba[2] << 8) | rgba[3];
		}
	}

	load_abgr8(out, pixels.data(), dw, dh);
	return true;
}

bool ui::set_user_background(const std::string& path)
{
	if(path.empty())
	{
		if(user_background_active)
		{
			delete_image(user_top_background);
			delete_image(user_bottom_background);
			user_top_background = { nullptr, nullptr };
			user_bottom_background = { nullptr, nullptr };
			user_background_active = false;
		}
		return true;
	}

	int width = 0, height = 0, channels = 0;
	if(!stbi_info(path.c_str(), &width, &height, &channels)
		|| width <= 0 || height <= 0 || width > 4096 || height > 4096
		|| (u64) width * height > 16000000ULL)
	{
		elog("background: unsupported dimensions for %s", path.c_str());
		return false;
	}
	u8 *bitmap = stbi_load(path.c_str(), &width, &height, &channels, 4);
	if(!bitmap)
	{
		elog("background: failed to decode %s", path.c_str());
		return false;
	}

	C2D_Image next_top = { nullptr, nullptr };
	C2D_Image next_bottom = { nullptr, nullptr };
	/* In 800px mode, cap the crop to the source width. If the wallpaper
	 * source is only 400px (designed for normal 3DS), don't upscale it
	 * — it'll be centred in the 800px framebuffer. */
	unsigned top_w = g_top_wide ? std::min<unsigned>(width, 800) : 400;
	bool ok = make_cover_image(&next_top, bitmap, width, height,
		top_w, 240)
		&& make_cover_image(&next_bottom, bitmap, width, height, 320, 240);
	stbi_image_free(bitmap);

	if(!ok)
	{
		if(next_top.tex) delete_image(next_top);
		if(next_bottom.tex) delete_image(next_bottom);
		return false;
	}

	if(user_background_active)
	{
		delete_image(user_top_background);
		delete_image(user_bottom_background);
	}
	user_top_background = next_top;
	user_bottom_background = next_bottom;
	user_top_bg_width = (float)top_w;
	user_background_active = true;
	ilog("background: loaded %s (%dx%d)", path.c_str(), width, height);
	return true;
}

bool ui::user_background_loaded()
{
	return user_background_active;
}

float ui::screen_width(ui::Screen scr)
{
	if(scr == ui::Screen::top)
		return g_top_wide ? 800.0f : ui::dimensions::width_top;
	return ui::dimensions::width_bottom;
}

static void draw_nocturne_chrome(ui::Screen screen)
{
	const float width = ui::screen_width(screen);
	if(user_background_active)
	{
		/* Wallpapers are atmosphere, never the content surface. This fixed
		 * scrim guarantees contrast even for pure-white photographs. */
		C2D_DrawRectSolid(0.0f, 0.0f, -0.80f, width, ui::screen_height(),
			C2D_Color32(0, 0, 0, get_nsettings()->wallpaper_dim));
	}
}

void ui::RenderQueue::terminate_render()
{
	int counter = 0;
	/* works by telling the global render queue to stop signaling other threads to stop
	 * and then wait 0.1 seconds so the currently being rendered frame is for sure done */
	ui::RenderQueue::global()->signal(ui::RenderQueue::signal_cancel);
	while(g_inRender && counter++ < 10) svcSleepThread(100000000ULL); /* 0.1 seconds */
	ui::RenderQueue::global()->unsignal(ui::RenderQueue::signal_cancel);
	/* after a second we can safely say it was a recursive panic or whatnot */
	ui::maybe_end_frame();
}

#ifndef RELEASE
static ui::RenderQueue *bound_queue = nullptr;
static std::string last_queue_desc;

static std::string widget_to_string(ui::BaseWidget *widget)
{
	if(widget->get_id() == ui::Text::id)
	{
		ui::Text *text = (ui::Text *) widget;
		return "Text(\"" + text->get_text() + "\")";
	}
	else
		return std::string(widget->get_id()) + "()";
}

static void swap_last_queue_desc(ui::RenderQueue *queue)
{
	if(bound_queue == queue) return;
	bound_queue = queue;

	last_queue_desc = "dump for render queue @ " + std::to_string((u32) queue) + ":\n";
	for(ui::BaseWidget *t : queue->top)
		last_queue_desc += " => (Top) " + widget_to_string(t) + "\n";
	for(ui::BaseWidget *t : queue->bot)
		last_queue_desc += " => (Bottom) " + widget_to_string(t) + "\n";
}

static void log_additional_dren_info(ui::RenderQueue *curqueue)
{
	dlog("previous queue:\n%s", last_queue_desc.c_str());
	swap_last_queue_desc(curqueue);
	dlog("current queue:\n%s", last_queue_desc.c_str());
}
#else
	#define swap_last_queue_desc(this)
	#define log_additional_dren_info(this)
#endif


enum enter_frame_ret {
	ParentReturnsFalse = 0,
	ParentReturnsTrue = 1,
	ParentContinues = 2,
};


#define RQ_ENTER_FRAME() \
	switch(this->enter_frame()) { \
		case ParentReturnsFalse: return false; \
		case ParentReturnsTrue: return true; \
		default: case ParentContinues: break; \
	} \

/* call with RQ_ENTER_FRAME() */
int ui::RenderQueue::enter_frame()
{
	if((this->signalBit | g_renderqueue.signalBit) & ui::RenderQueue::signal_cancel)
		return ParentReturnsFalse;

	/* this means the application was closed from the home menu and HMM is waiting for
	 * the application to gracefully die */
	if(!aptMainLoop())
		::exit(0);

	bool isOpen;

	{
		ctr::LockedInScope scope_lock { &in_render_lock };
		if(g_inRender)
		{
			log_additional_dren_info(this);
			panic("illegal double render");
		}

		if (g_renderqueue.before_render_begin != nullptr)
		{
			std::function<void()> *cb = g_renderqueue.before_render_begin;
			g_renderqueue.before_render_begin = nullptr;
			(*cb)();
			delete cb;
		}

		swap_last_queue_desc(this);

		/* not rendering if the shell is closed */
		if(R_SUCCEEDED(ui::shell_is_open(&isOpen)))
		{
			/* this must be here due to LEDExpireTime; it must work during sleep */
			if((isOpen && (LEDFlags & LED_RESET_SLEEP)) || ((LEDFlags & LED_TIMEOUT) && time(NULL) > LEDExpireTime))
			{
				ui::LED::ResetPattern();
				ui::LED::ClearResetFlags();
			}
			if(!isOpen)
			{
				bool ret = true;
				for(ui::BaseWidget *wid : this->sleepProcessors)
					ret &= wid->process_in_sleep();
				return ret ? ParentReturnsTrue : ParentReturnsFalse;
			}
		}

		g_inRender = true;
	}

	if(!C3D_FrameBegin(C3D_FRAME_SYNCDRAW))
		panic("failed to start frame");

	C2D_TargetClear(g_top, slotmgr.get(0));
	C2D_TargetClear(g_bot, slotmgr.get(0));

	return ParentContinues;
}

/* AND this with the ret variable */
bool ui::RenderQueue::exit_frame()
{
	ui::maybe_end_frame();
	bool ret = true;

	if(g_renderqueue.after_render_complete != nullptr)
	{
		std::function<bool()> *func = g_renderqueue.after_render_complete;
		g_renderqueue.after_render_complete = nullptr;
		ret &= (*func)();
		delete func;
	}

	return ret;
}

/* does not also render the global render queue */
bool ui::RenderQueue::render_exclusive_frame(ui::Keys& keys)
{
	RQ_ENTER_FRAME();
	bool ret = true;

	begin_top_scene();
	draw_top_background(keys);
	draw_nocturne_chrome(ui::Screen::top);
	ret &= this->render_top(keys);

	begin_bottom_scene();
	if(user_background_active)
		C2D_DrawImageAt(user_bottom_background, 0.0f, 0.0f, -0.9f);
	else if(bottom_background->has_image())
		bottom_background->render(keys);
	draw_nocturne_chrome(ui::Screen::bottom);
	ret &= this->render_bottom(keys);

	ret &= this->exit_frame();
	return ret;
}

/* render the current render queue alongside the global renderqueue */
bool ui::RenderQueue::render_frame(ui::Keys& keys)
{
	RQ_ENTER_FRAME();
	bool ret = true;

	begin_top_scene();
	draw_top_background(keys);
	draw_nocturne_chrome(ui::Screen::top);
	ret &= this->render_top(keys);
	ret &= g_renderqueue.render_top(keys);

	begin_bottom_scene();
	if(user_background_active)
		C2D_DrawImageAt(user_bottom_background, 0.0f, 0.0f, -0.9f);
	else if(bottom_background->has_image())
		bottom_background->render(keys);
	draw_nocturne_chrome(ui::Screen::bottom);
	ret &= this->render_bottom(keys);
	ret &= g_renderqueue.render_bottom(keys);

	ret &= this->exit_frame();
	return ret;
}

/* set up a callback before the global render queue renders again */
void ui::RenderQueue::before_render(std::function<void()> cb)
{
	panic_assert(this == &g_renderqueue, "can only apply before-render strategory to global RenderQueue");
	LightLock_Lock(&before_render_lock);
	panic_assert(this->before_render_begin == nullptr, "illegal double before_render_begin");
	this->before_render_begin = new std::function<void()>(cb);
	LightLock_Unlock(&before_render_lock);
}

/* set up a callback that runs as soon as the next global render queue's frame has finished rendering */
void ui::RenderQueue::render_and_then(std::function<bool()> cb)
{
	panic_assert(this == &g_renderqueue, "can only apply render-and-then strategy to global RenderQueue");
	LightLock_Lock(&render_and_then_lock);
	panic_assert(this->after_render_complete == nullptr, "illegal double after_render_complete");
	this->after_render_complete = new std::function<bool()>(cb);
	LightLock_Unlock(&render_and_then_lock);
}

void ui::RenderQueue::render_and_then(std::function<void()> cb)
{ this->render_and_then(std::function<bool()>([cb]() { cb(); return true; })); }

/* remove a callback installed for next global render queue's frame */
void ui::RenderQueue::detach_after()
{
	if(this->after_render_complete != nullptr)
	{
		delete this->after_render_complete;
		this->after_render_complete = nullptr;
	}
}

void ui::RenderQueue::signal(u8 bits)
{ this->signalBit |= bits; }

void ui::RenderQueue::unsignal(u8 bits)
{ this->signalBit &= ~bits; }

void ui::RenderQueue::trigger_i18n_update(lang::type nlang)
{
	for (ui::BaseWidget *wid : this->top) {
		if (wid->i18n_update_setup())
			wid->i18n_update_cb(nlang);
	}

	for (ui::BaseWidget *wid : this->bot) {
		if (wid->i18n_update_setup())
			wid->i18n_update_cb(nlang);
	}

	for (ui::BaseWidget *wid : this->sleepProcessors) {
		if (wid->i18n_update_setup())
			wid->i18n_update_cb(nlang);
	}
}

bool ui::RenderQueue::render_frame()
{
	ui::Keys keys = ui::RenderQueue::get_keys();
	return this->render_frame(keys);
}

bool ui::RenderQueue::render_screen(ui::Keys& keys, ui::Screen screen)
{
	return screen == ui::Screen::top ? this->render_top(keys) : this->render_bottom(keys);
}

bool ui::RenderQueue::render_list(std::list<ui::BaseWidget *>& list, ui::Keys& keys)
{
	static ui::Keys zeroKeys = { };

	bool ret = true;
	for(ui::BaseWidget *wid : list)
		if(!wid->is_hidden())
		{
			ret &= wid->render(g_renderqueue.exclusiveInput
				? (wid->has_exclusive_input() ? keys : zeroKeys)
				: keys);
		}
	return ret;
}

bool ui::RenderQueue::render_bottom(ui::Keys& keys)
{
	return this->render_list(this->bot, keys);
}

bool ui::RenderQueue::render_top(ui::Keys& keys)
{
	return this->render_list(this->top, keys);
}

void ui::RenderQueue::render_forever()
{
	while(true) this->render_frame();
}

void ui::RenderQueue::render_until_button(u32 kDownMask)
{
	ui::Keys keys = ui::RenderQueue::get_keys();
	while((keys.kDown & kDownMask) == 0)
		this->render_frame(keys), keys = ui::RenderQueue::get_keys();
}

void ui::RenderQueue::render_finite()
{
	while(this->render_frame())
		;
}

void ui::RenderQueue::render_finite_button(u32 kDownMask)
{
	ui::Keys keys = ui::RenderQueue::get_keys();
	while((keys.kDown & kDownMask) == 0 && this->render_frame(keys))
		keys = ui::RenderQueue::get_keys();
}

ui::RenderQueue *ui::RenderQueue::global()
{ return &g_renderqueue; }

std::list<ui::RenderQueue *>& ui::RenderQueue::get_i18n_register()
{ return g_i18n_rq_register; }

/* class SpriteStore */

ui::SpriteStore::SpriteStore(const std::string& fname)
{
	this->open(fname);
}

ui::SpriteStore::~SpriteStore()
{
	if(this->sheet != nullptr)
		C2D_SpriteSheetFree(this->sheet);
}

void ui::SpriteStore::open(const std::string& fname)
{
	this->sheet = C2D_SpriteSheetLoad(fname.c_str());
	panic_assert(this->sheet != nullptr, "failed to open spritesheet");
}

size_t ui::SpriteStore::size()
{
	return C2D_SpriteSheetCount(this->sheet);
}

C2D_Sprite ui::SpriteStore::get_by_id(ui::sprite id)
{
	C2D_Sprite ret;
	C2D_SpriteFromSheet(&ret, g_spritestore.sheet, (size_t) id);
	return ret;
}


/* core widget class Text */

#define TEXT_MAXLINES 16

UI_SLOTS(ui::Text_color, color_text)

void ui::Text::setup(str::type i18n_strid)
{
	this->set_text(i18n::getstr(i18n_strid));
	this->label_strid = i18n_strid;
	this->i18n_auto_update_setup();
}

void ui::Text::setup(const std::string& label)
{
	this->text = label;
	this->prepare_arrays();
}

void ui::Text::setup()
{
	this->prepare_arrays();
}

void ui::Text::prepare_arrays()
{
	if(this->buf == nullptr)
	{
		this->buf = C2D_TextBufNew(this->text.size() + TEXT_MAXLINES + 1);
		this->lines.reserve(TEXT_MAXLINES);
	}
	else
	{
		// Reset
		C2D_TextBufClear(this->buf);
		this->buf = C2D_TextBufResize(this->buf, this->text.size() + TEXT_MAXLINES + 1);
		this->lines.clear();
	}

	float width = this->maxw ? this->maxw : ui::screen_width(this->screen) - this->x;

	float curWidth = 0;
	char codepoint[5] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	u8 expect = 0;
	u8 cpc = 0;
	std::string cur;
	/* should be enough for a line */
	cur.reserve(128);
	C2D_TextBuf linebuf = C2D_TextBufNew(1024);
	C2D_Text linetext;

	for(size_t i = 0; i < this->text.size(); ++i)
	{
		if((this->doAutowrap && width - curWidth < 10.0f) || this->text[i] == '\n')
		{
			curWidth = 0;
			if(!isspace(this->text[i]))
			{
				std::string::size_type last_space = cur.rfind(' ');
				/* means the whole line was taken up by one word....
				 * we can only make it wrap then */
				if(last_space != std::string::npos)
				{
					std::string::size_type discarded = cur.size() - last_space;
					cur.erase(last_space);
					i -= discarded;
				}
			}
			this->push_str(cur);
			cur.clear();

			/* makes it so that you don't get random indents if
			 * the next character is a space, or in the case of
			 * a newline doesn't create a new paragraph */
			if(isspace(this->text[i]))
				continue;
		}

		u8 code = this->text[i];
		if(cpc == 0)
		{
			codepoint[cpc++] = code;
			if(code < 0x80)
				; /* single-byte; immediately process */
			/* doesn't check for actual validity but this works well enough */
			else if(code < 0xE0) { expect = 2; continue; }
			else if(code < 0xF0) { expect = 3; continue; }
			else if(code < 0xF5) { expect = 4; continue; }
		}
		else if(cpc != expect)
		{
			codepoint[cpc++] = code;
			if(cpc != expect)
				continue;
			codepoint[cpc] = '\0';
		}

		cur.append(codepoint, cpc);

		if(this->doAutowrap)
		{
			/* this seems inefficient, is there a better way to do this? */
			ui::parse_text(&linetext, linebuf, cur.c_str());
			C2D_TextGetDimensions(&linetext, this->xsiz, this->ysiz, &curWidth, nullptr);
			curWidth += 10.0f; /* small margin */
			C2D_TextBufClear(linebuf);
		}

		codepoint[0] = '\0';
		expect = 0;
		cpc = 0;
	}

	if(expect != cpc)
		elog("Incomplete utf-8 data passed. Not appending final codepoint.");

	if(cur.size() > 0) this->push_str(cur);
	if(this->lines.size() > 0)
	{
		C2D_TextGetDimensions(&this->lines.front(), this->xsiz, this->ysiz, nullptr,
			&this->lineHeight);
	}
	else this->lineHeight = 0;
	C2D_TextBufDelete(linebuf);
}

void ui::Text::push_str(const std::string& str)
{
	if(this->lines.size() + 1 == TEXT_MAXLINES)
	{
		elog("too many lines to handle, discarding line '%s'", str.c_str());
		return;
	}
	this->lines.emplace_back();
	ui::parse_text(&this->lines.back(), this->buf, str.c_str());
	C2D_TextOptimize(&this->lines.back());
}

void ui::Text::destroy()
{
	C2D_TextBufDelete(this->buf);
	if (this->i18n_additional_cb != nullptr)
		delete this->i18n_additional_cb;
}

static float get_center_x(C2D_Text *txt, float xsiz, float ysiz, ui::Screen screen)
{
	float width; C2D_TextGetDimensions(txt, xsiz, ysiz, &width, nullptr);
	return ((ui::screen_width(screen) / 2) - (width / 2.0f));
}

bool ui::Text::render(ui::Keys&)
{
	if(this->doScroll)
	{
		if(this->sctx.offset > this->sctx.width)
			this->reset_scroll();
		else if(this->sctx.offset == 0)
		{
			if(this->sctx.timing > 20)
				++this->sctx.offset;
			++this->sctx.timing;
		}
		else if(this->sctx.timing % 3 == 0)
		{
			this->sctx.timing = 1;
			++this->sctx.offset;
			--this->sctx.rx;
		}
		else ++this->sctx.timing;

		ui::background_rect(this->screen, 0, this->y, 0.1f, this->x, this->sctx.height);
	}

	float xd = this->doScroll ? this->sctx.rx : this->x;
	for(size_t i = 0; i < this->lines.size(); ++i)
	{
		if(this->drawCenter)
		{
			float width;
			C2D_TextGetDimensions(&this->lines[i], this->xsiz, this->ysiz, &width, nullptr);
			xd = (this->maxw ? this->maxw : ui::screen_width(this->screen)) / 2.0f - width / 2.0f;
			/* If maxw is set let's just assume it's a box and that we don't still want the
			 * screen center to be screen_width / 2 */
			if(this->maxw) xd += this->x;

		}
		/* Fractional coordinates soften the system font on the physical
		 * 3DS LCD. Snap each line to the pixel grid for cleaner glyphs. */
		float draw_x = std::round(xd);
		float draw_y = std::round(this->y + (this->lineHeight * i));
		C2D_DrawText(&this->lines[i], C2D_WithColor, draw_x,
			draw_y, this->z, this->xsiz, this->ysiz, this->slots[0]);
	}

	return true;
}

void ui::Text::reset_scroll()
{
	this->sctx.height = this->height();
	this->sctx.width = this->width();
	this->sctx.rx = this->x;
	this->sctx.timing = 1;
	this->sctx.offset = 0;
}

void ui::Text::autowrap()
{
	this->doAutowrap = true;
}

void ui::Text::finalize()
{
	this->prepare_arrays();
}

void ui::Text::scroll()
{
	this->reset_scroll();
	// Only scroll if required
	if(this->sctx.width + this->x > ui::screen_width(this->screen))
	{
		this->doScroll = true;
		this->z = 0.0f;
	}
}

void ui::Text::set_center_x()
{
	this->drawCenter = true;
}

float ui::Text::height()
{
	return this->lines.size() * this->lineHeight;
}

float ui::Text::width()
{
	float ret = 0.0f;
	for(C2D_Text& line : this->lines)
	{
		float width;
		C2D_TextGetDimensions(&line, this->xsiz, this->ysiz, &width, nullptr);
		if(width > ret) ret = width;
	}
	return ret;
}

void ui::Text::resize(float x, float y)
{
	this->xsiz = x;
	this->ysiz = y;
	if(this->lines.size())
	{
		C2D_TextGetDimensions(&this->lines.front(), this->xsiz, this->ysiz, nullptr,
			&this->lineHeight);
	}
}

void ui::Text::set_max_width(float w)
{
	this->maxw = w;
}

const std::string& ui::Text::get_text()
{ return this->text; }

str::type ui::Text::get_i18n_strid()
{ return this->label_strid; }

void ui::Text::set_text(str::type labelid)
{
	this->setup(labelid);
}

void ui::Text::set_text(const std::string& label)
{
	this->text = label;
	this->prepare_arrays();
}

void ui::Text::swap_slots(const StaticSlot& slot)
{
	UI_THIS_SWAP_SLOTS(slots);
}

void ui::Text::i18n_auto_update_setup()
{
	this->i18n_update_cb = [this](lang::type nlang) -> void {
		(void)nlang;
		this->set_text(i18n::getstr(this->label_strid, nlang));

	};
	this->does_i18n_update = true;
}

/* core widget class Sprite */

void ui::Sprite::setup(std::function<void(C2D_Sprite&, u32)> get_cb, u32 data)
{
	ui::ThemeManager::global()->get_slots(this, "Sprite", 0, nullptr);
	memset(&this->sprite.params, 0, sizeof(C2D_DrawParams));
	memset(&this->tint, 0, sizeof(C2D_ImageTint));
	this->unspecified_data = data;
	this->get_sprite_func = get_cb;
	this->set_center(0.0f, 0.0f);
	this->set_opacity(1.0f);
	this->update_theme_hook();
}

void ui::Sprite::set_data(u32 data)
{
	this->unspecified_data = data;
	this->update_theme_hook();
}

float ui::Sprite::height()
{
	return this->sprite.image.subtex->height;
}

float ui::Sprite::width()
{
	return this->sprite.image.subtex->width;
}

bool ui::Sprite::render(ui::Keys& keys)
{
	((void) keys); // unused

	C2D_DrawSpriteTinted(&this->sprite, &this->tint);

	return true;
}

void ui::Sprite::set_x(float x)
{
	this->x = ui::transform(this, x);
	C2D_SpriteSetPos(&this->sprite, x, this->y);
}

void ui::Sprite::set_y(float y)
{
	this->y = ui::transform(this, y);
	C2D_SpriteSetPos(&this->sprite, this->x, this->y);
}

void ui::Sprite::set_z(float z)
{
	this->z = z;
	C2D_SpriteSetDepth(&this->sprite, z);
}

void ui::Sprite::set_opacity(float value)
{
	C2D_AlphaImageTint(&this->tint, value);
}

void ui::Sprite::rotate(float degs)
{
	C2D_SpriteRotateDegrees(&this->sprite, degs);
}

void ui::Sprite::set_center(float x, float y)
{
	C2D_SpriteSetCenter(&this->sprite, x, y);
}

void ui::Sprite::update_theme_hook()
{
	this->get_sprite_func(this->sprite, this->unspecified_data);
}

/* core widget class Button */

UI_SLOTS(ui::Button_colors, color_button_border, color_button)

void ui::Button::i18n_auto_update_setup()
{
	this->i18n_update_cb = [this](lang::type nlang) -> void {
		ui::BaseWidget *sub_wid = this->get_widget();
		if (sub_wid && sub_wid->i18n_update_setup()) {
			sub_wid->i18n_update(nlang);
			if (this->does_autowrap) this->autowrap();
		}
		this->readjust();
	};
	this->does_i18n_update = true;
}

void ui::Button::setup(const std::string& text)
{
	this->widget = nullptr;
	this->enabled = true;
	this->set_label(text);
}

void ui::Button::setup(str::type i18n_strid)
{
	this->widget = nullptr;
	this->enabled = true;
	this->set_label(i18n_strid);
	this->i18n_strid = i18n_strid;
	this->i18n_auto_update_setup();
}

void ui::Button::setup(std::function<void(C2D_Sprite&, u32)> get_image_cb, u32 data)
{
	ui::Sprite *label = new ui::Sprite(this->screen);
	label->setup(get_image_cb, data);
	label->set_z(ui::layer::image);
	label->finalize();

	this->widget = label;
	this->enabled = true;
	this->readjust();
}

void ui::Button::setup()
{ this->setup(""); }

void ui::Button::set_enabled(bool enabled)
{ this->enabled = enabled; }

void ui::Button::set_label(const std::string& v)
{
	if(this->widget != nullptr)
	{
		this->widget->destroy();
		delete this->widget;
	}

	ui::Text *label = new ui::Text(this->screen);
	label->setup(v);
	label->finalize();

	this->widget = label;
	this->readjust();
}

void ui::Button::set_label(str::type strid)
{
	if(this->widget != nullptr)
	{
		this->widget->destroy();
		delete this->widget;
	}

	ui::Text *label = new ui::Text(this->screen);
	label->setup(strid);
	label->finalize();
	label->i18n_auto_update_setup();

	this->widget = label;
	this->readjust();
}

const std::string& ui::Button::get_label()
{
	return ((ui::Text *) this->widget)->get_text();
}

void ui::Button::destroy()
{
	this->widget->destroy();
	delete this->widget;
}

float ui::Button::height()
{ return this->h; }

float ui::Button::width()
{ return this->w; }

void ui::Button::resize(float x, float y)
{
	this->w = x;
	this->h = y;
	this->readjust();
}

void ui::Button::resize_children(float x, float y)
{
	// Super unsafe... works though
	((ui::Text *) this->widget)->resize(x, y);
	this->readjust();
}

void ui::Button::set_x(float x)
{
	this->x = ui::transform(this, x);
	this->readjust();
}

void ui::Button::set_y(float y)
{
	this->y = ui::transform(this, y);
	this->readjust();
}

void ui::Button::set_z(float z)
{
	this->z = z;
	this->widget->set_z(z + 0.1f);
}

void ui::Button::set_border(bool b)
{
	this->showBorder = b;
}

void ui::Button::readjust()
{
	/* center the subwidget in the container */
	this->widget->set_y(((this->h / 2) - (this->widget->height() / 2)) + this->y);
	this->widget->set_x(((this->w / 2) - (this->widget->width() / 2)) + this->x);

	this->ox = this->x + this->w;
	this->oy = this->y + this->h;
}

bool ui::Button::press()
{
	if (this->enabled)
		return this->on_click(this);

	return true;
}

bool ui::Button::render(ui::Keys& keys)
{
	if(this->showBorder) C2D_DrawRectSolid(this->x - 1, this->y - 1, 0.0f, this->w + 2, this->h + 2, this->slots.get(0));
	if(this->showBg) C2D_DrawRectSolid(this->x, this->y, 0.1f, this->w, this->h, this->slots.get(1));
	this->widget->render(keys);

	if(ui::is_touched(keys) && keys.touch.px >= this->x && keys.touch.px <= this->ox &&
			keys.touch.py >= this->y && keys.touch.py <= this->oy)
	{
		ui::set_touch_lock(keys);
		return this->press();
	}

	return true;
}

void ui::Button::autowrap()
{
	this->does_autowrap = true;
	this->w = this->widget->width() + (this->showBg ? 6.0f : 10.0f);
	this->h = this->widget->height() + (this->showBg ? 2.0f : 2.0f);
	this->readjust();
}

float ui::Button::textwidth()
{
	return this->widget->width();
}

void ui::Button::swap_slots(const StaticSlot& slot)
{
	this->widget->swap_slots(slot);
}

/* core widget class ButtonCallback */

void ui::ButtonCallback::setup(u32 keys)
{ this->keys = keys; }

bool ui::ButtonCallback::render(ui::Keys& keys)
{
	if((keys.kDown & this->keys) && !this->cb_down(keys.kDown)) return false;
	if((keys.kHeld & this->keys) && !this->cb_held(keys.kHeld)) return false;
	if((keys.kUp & this->keys) && !this->cb_up(keys.kUp)) return false;
	return true;
}

/* core widget class Toggle */

#define SLIDER_X(t) (t->toggled_state ? (t->x + 2) + (t->width() - 4) / 2 : (t->x + 2))
#define SLIDER_Y(t) (t->y + 2)
#define SLIDER_WIDTH(t) ((t->width() - 4) / 2)
#define SLIDER_HEIGHT(t) (t->height() - 4)

UI_SLOTS(ui::Toggle_color, color_toggle_green, color_toggle_red, color_toggle_slider)

void ui::Toggle::setup(bool state, std::function<void()> on_toggle_cb)
{
	this->toggled_state = state;
	this->toggle_cb = on_toggle_cb;
}

void ui::Toggle::set_toggled(bool toggled)
{
	this->toggled_state = toggled;
}

void ui::Toggle::toggle(bool toggled)
{
	this->toggled_state = toggled;
	this->toggle_cb();
}

bool ui::Toggle::render(ui::Keys& keys)
{
	if((keys.touch.px >= SLIDER_X(this) && keys.touch.px <= SLIDER_X(this) + SLIDER_WIDTH(this)) && (keys.touch.py >= SLIDER_Y(this) && keys.touch.py <= SLIDER_Y(this) + SLIDER_HEIGHT(this)) && (osGetTime() - this->last_touch_toggle) >= 300)
	{
		this->toggle(!this->toggled_state);
		this->last_touch_toggle = osGetTime();
	}

	/* Subtle pink border for definition against dark backgrounds */
	C2D_DrawRectSolid(this->x - 1, this->y - 1, this->z - 0.01f, this->width() + 2, this->height() + 2,
		C2D_Color32(255, 164, 204, 32));
	/* Toggle track */
	C2D_DrawRectSolid(this->x, this->y, this->z, this->width(), this->height(), this->slots.get(this->toggled_state ? 0 : 1));
	/* Toggle slider with slight shadow for depth */
	C2D_DrawRectSolid(SLIDER_X(this), SLIDER_Y(this), this->z + 0.01f, SLIDER_WIDTH(this), SLIDER_HEIGHT(this), this->slots.get(2));

	return true;
}

/* WidgetGroup */

void ui::WidgetGroup::add(ui::BaseWidget *w)
{
	this->ws.push_back(w);
}

void ui::WidgetGroup::set_hidden(bool b)
{
	for(ui::BaseWidget *w : this->ws)
		w->set_hidden(b);
}

void ui::WidgetGroup::set_y_descending(float pos, float elpadding)
{
	for(ui::BaseWidget *w : this->ws)
	{
		w->set_y(pos);
		pos += elpadding;
		pos += w->height();
	}
}

void ui::WidgetGroup::position_under(ui::BaseWidget *other, float initpad, float elpadding)
{
	if(!this->ws.size()) return;
	this->set_y_descending(ui::under(other, this->ws[0], initpad), elpadding);
}

void ui::WidgetGroup::position_under_horizontal(ui::BaseWidget *other, float pad)
{
	/* first we get the highest widget so we can center all around it */
	float middle, hmax = 0.0f, h;
	for(ui::BaseWidget *w : this->ws)
		if((h = w->height()) > hmax)
			hmax = h;
	middle = other->get_y() + other->height() + pad + (hmax / 2.0f);
	for(ui::BaseWidget *w : this->ws)
		w->set_y(middle - (w->height() / 2.0f));
}

#define DEFINE_GROUP_MAXMIN_FUNC(func_name, prop_get, cmp, init_val) \
	ui::BaseWidget *ui::WidgetGroup::func_name() \
	{ \
		if(!this->ws.size()) return nullptr; \
		ui::BaseWidget *ret = nullptr; float max = init_val, v; \
		for(ui::BaseWidget *w : this->ws) \
			if((v = prop_get) cmp max) \
			{ \
				max = v; \
				ret = w; \
			} \
		return ret; \
	}

DEFINE_GROUP_MAXMIN_FUNC(max_height, w->height(), >, 0.0f)
DEFINE_GROUP_MAXMIN_FUNC(min_height, w->height(), <, this->ws[0]->height())
DEFINE_GROUP_MAXMIN_FUNC(max_width, w->width(), >, 0.0f)
DEFINE_GROUP_MAXMIN_FUNC(min_width, w->width(), <, this->ws[0]->width())
DEFINE_GROUP_MAXMIN_FUNC(highest, w->get_y(), <, this->ws[0]->get_y()) /* misleading! intentional */
DEFINE_GROUP_MAXMIN_FUNC(lowest, w->get_y() + w->height(), >, 0.0f) /* misleading! intentional */
DEFINE_GROUP_MAXMIN_FUNC(rightmost, w->get_x() + w->width(), >, 0.0f)
DEFINE_GROUP_MAXMIN_FUNC(leftmost, w->get_x(), <, this->ws[0]->get_x())

#undef DEFINE_GROUP_MAXMIN_FUNC

void ui::WidgetGroup::translate(float x, float y)
{
	for(ui::BaseWidget *w : this->ws)
	{
		w->set_x(w->get_x() + x);
		w->set_y(w->get_y() + y);
	}
}

bool ui::WidgetGroup::render_all(ui::Screen screen, ui::Keys& keys)
{
	bool ret = true;
	for(ui::BaseWidget *w : this->ws)
		if(w->renders_on() == screen)
			ret &= w->render(keys);
	return ret;
}

bool ui::WidgetGroup::render_all(ui::Keys& keys)
{
	bool ret = true;
	for(ui::BaseWidget *w : this->ws)
		ret &= w->render(keys);
	return ret;
}

void ui::WidgetGroup::destroy_all()
{
	for(ui::BaseWidget *w : this->ws)
	{
		w->destroy();
		delete w;
	}
}

/* LED */

void ui::LED::Solid(ui::LED::Pattern *info, u32 animation, u8 r, u8 g, u8 b)
{
	info->animation = animation;
	memset(info->red_pattern,   r, 32);
	memset(info->green_pattern, g, 32);
	memset(info->blue_pattern,  b, 32);
}

Result ui::LED::SetPattern(ui::LED::Pattern *info)
{
	if(!ISET_ALLOW_LED)
		return 0; /* no-op is success */

	Result res;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = 0x08010640; // https://www.3dbrew.org/wiki/PTMSYSM:SetInfoLEDPattern
	cmdbuf[1] = info->animation;
	memcpy(&cmdbuf[ 2], info->red_pattern  , 32);
	memcpy(&cmdbuf[10], info->green_pattern, 32);
	memcpy(&cmdbuf[18], info->blue_pattern , 32);

	if(R_FAILED(res = svcSendSyncRequest(*ptmSysmGetSessionHandle())))
		return res;
	return (Result) cmdbuf[1];
}

Result ui::LED::SetSleepPattern(ui::LED::Pattern *info)
{
	bool isOpen;
	if(R_FAILED(ui::shell_is_open(&isOpen)) || isOpen)
		return 0; /* no-op is success */
	Result res;
	if(R_SUCCEEDED(res = ui::LED::SetPattern(info)))
		LEDFlags |= LED_RESET_SLEEP;
	return res;
}

void ui::LED::SetTimeout(time_t newTime)
{
	LEDFlags |= LED_TIMEOUT;
	LEDExpireTime = newTime;
}

void ui::LED::ClearResetFlags(void)
{
	/* functionally equivalent to LEDFlags = LED_NONE currently */
	LEDFlags &= ~(LED_TIMEOUT | LED_SLEEP_MODE);
}

Result ui::LED::ResetPattern()
{
	ui::LED::Pattern info;
	memset(&info, 0, sizeof(info));
	return ui::LED::SetPattern(&info);
}

/* BackgroundObscurer */

void ui::detail::BackgroundObscurer::setup(float opacity)
{
	/* highest byte is the alpha component */
	this->colour = (u32) (opacity * 0xFF) << 24;
}

bool ui::detail::BackgroundObscurer::render(ui::Keys&)
{
	C2D_DrawRectSolid(0.0f, 0.0f, ui::layer::under_image, ui::screen_width(this->screen), ui::screen_height(), this->colour);
	return true;
}

bool ui::is_obscured()
{
	return !g_renderqueue.find_tag(ui::tag::obscure_bottom)->is_hidden();
}

void ui::obscure(bool enable)
{
	ui::detail::BackgroundObscurer
		*bottom = g_renderqueue.find_tag<ui::detail::BackgroundObscurer>(ui::tag::obscure_bottom),
		*top = g_renderqueue.find_tag<ui::detail::BackgroundObscurer>(ui::tag::obscure_top);

	bottom->set_hidden(!enable);
	top->set_hidden(!enable);
}

bool ui::is_exclusive_input()
{
	return g_renderqueue.is_exclusive_input();
}

void ui::exclusive_input(bool enable)
{
	g_renderqueue.set_exclusive_input(enable);
}
