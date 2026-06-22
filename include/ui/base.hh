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

#ifndef inc_ui_base_hh
#define inc_ui_base_hh

// Defines constants, we copy them over in
// our enum later
#include "build/next.h"

#include <ui/theme.hh>
#include <panic.hh>

#include <citro3d.h>
#include <citro2d.h>
#include <3ds.h>

#include <functional>
#include <sstream>
#include <vector>
#include <string>
#include <list>

#include "i18n.hh"

#define UI_WIDGET(name) \
	public: \
		using ui::BaseWidget::BaseWidget; \
		static constexpr const char *id = name; \
		const char *get_id() override { return name; } \
	private:

#define UI_BUILDER_EXTENSIONS() \
	template <typename TWidget> \
	class Internal__MyBuilder : public ui::detail::builder<TWidget, Internal__MyBuilder<TWidget>>
#define UI_USING_BUILDER() \
	public: using ui::detail::builder<TWidget, Internal__MyBuilder<TWidget>>::builder; \
	private: using ReturnValue = Internal__MyBuilder<TWidget>&; \
	private: TWidget& instance() { return *this->el; } \
	protected: ReturnValue return_value() override { return *this; } \
	private:

#define UI_COLOR(r,g,b,a) \
	0x##a##b##g##r

#define UI_THIS_SWAP_SLOTS(slots) \
	this->slots = ui::ThemeManager::global()->get_slots(this, (slot).id, (slot).count, (slot).getters);

// Button glyphs
#define UI_GLYPH_A               "\uE000"
#define UI_GLYPH_B               "\uE001"
#define UI_GLYPH_X               "\uE002"
#define UI_GLYPH_Y               "\uE003"
#define UI_GLYPH_L               "\uE004"
#define UI_GLYPH_R               "\uE005"
#define UI_GLYPH_DPAD_CLEAR      "\uE006"
#define UI_GLYPH_CPAD            "\uE077"
#define UI_GLYPH_DPAD_UP         "\uE079"
#define UI_GLYPH_DPAD_DOWN       "\uE07A"
#define UI_GLYPH_DPAD_LEFT       "\uE07B"
#define UI_GLYPH_DPAD_RIGHT      "\uE07C"
#define UI_GLYPH_DPAD_VERTICAL   "\uE07D"
#define UI_GLYPH_DPAD_HORIZONTAL "\uE07E"

#define UI_LED_MAKE_ANIMATION(delay, smoothing, loop_delay) \
	((((delay) & 0xFF) << 24) | (((smoothing) & 0xFF) << 16) | (((loop_delay) & 0xFF) << 8))

/* from supplement_sysfont_merger.c */
extern "C" C2D_Font ui__sysFontWithSupplement;

namespace ui
{
	namespace LED
	{
		typedef struct Pattern {
			u32 animation;
			u8 red_pattern[32];
			u8 green_pattern[32];
			u8 blue_pattern[32];
		} Pattern;

		void Solid(Pattern *info, u32 animation, u8 r, u8 g, u8 b);
		inline void Solid(Pattern *info, u32 animation, u32 abgr)
		{ Solid(info, animation, (abgr) & 0xFF, (abgr >> 8) & 0xFF, (abgr >> 16) & 0xFF); }
		Result SetSleepPattern(Pattern *info);
		Result SetPattern(Pattern *info);
		void SetTimeout(time_t newTime);
		Result ResetPattern(void);
		void ClearResetFlags(void);
	}

	class BaseWidget; /* forward declaration */

	enum class Screen
	{
		top, bottom,
		/* to be used by utility widgets */
		none
	};

	namespace dimensions
	{
		constexpr float height       = 240.0f; /* height of both screens */
		constexpr float width_bottom = 320.0f; /* width of the bottom screen */
		constexpr float width_top    = 400.0f; /* width of the top screen */
	}

	namespace layout
	{
		constexpr float center_x = -1.0f; /* center x position */
		constexpr float center_y = -2.0f; /* center y position */
		constexpr float left     =  3.0f; /* left of the screen */
		constexpr float right    = -4.0f; /* right of the screen */
		constexpr float top      =  3.0f; /* top of the screen */
		constexpr float bottom   = -6.0f; /* bottom of the screen */
		constexpr float base     = 35.0f; /* base heigth */
	}

	namespace layer
	{
		constexpr float bottom      = 0.1f;
		constexpr float middle      = 0.5f;
		constexpr float under_image = 0.80f;
		constexpr float image       = 0.81f;
		constexpr float above_image = 0.82f;
		constexpr float top         = 0.9f;
	}

	namespace tag
	{
		constexpr int action         = -1;  /* action header */
		constexpr int more           = -2;  /* more button */
		constexpr int settings       = -3;  /* settings button */
		constexpr int search         = -4;  /* search button */
		constexpr int queue          = -5;  /* queue button */
		constexpr int konami         = -6;  /* konami listner */
		constexpr int free_indicator = -7;  /* free space indicator */
		constexpr int random         = -8;  /* random button */
		constexpr int status         = -9;  /* status line */
		constexpr int net_indicator  = -10; /* network indicator */
		constexpr int obscure_bottom = -11; /* bottom obscurer */
		constexpr int obscure_top    = -12; /* top obscurer */
	};

	/* holds sprite ids used for ui::SpriteStore::get_by_id() */
	enum class sprite : u32
	{
		bun = next_bun_idx,
#undef next_bun_idx
		logo = next_logo_idx,
#undef next_logo_idx
		net_discon = next_net_discon_idx,
#undef next_net_discon_idx
		net_0 = next_net_0_idx,
#undef next_net_0_idx
		net_1 = next_net_1_idx,
#undef next_net_1_idx
		net_2 = next_net_2_idx,
#undef next_net_2_idx
		net_3 = next_net_3_idx,
#undef next_net_3_idx
		gplv3 = next_gplv3_idx,
#undef next_gplv3_idx
	};

	struct Keys
	{
		u32 kDown, kHeld, kUp;
		touchPosition touch;
	};

	typedef union {
		char *str;
		str::type strid;
	} desc_type;

	typedef struct prev_desc
	{
		desc_type desc;
		int does_i18n;

		~prev_desc() {
			if (!this->does_i18n && this->desc.str) {
				free(this->desc.str);
			}
		}
	} prev_desc;

	inline const char *parse_text(C2D_Text *text, C2D_TextBuf buf, const char *str)
	{
		return C2D_TextFontParse(text, ui__sysFontWithSupplement, buf, str);
	}

	typedef void (*select_command_handler)(u32 kHeld);
	void set_select_command_handler(select_command_handler handler);

	void set_touch_lock(ui::Keys& keys);
	void set_touch_lock();
	void scan_keys();
	u32 kDown();
	u32 kHeld();

	/* do not use */
	void maybe_end_frame();

	/* is there a way we can avoid doing this entirely? */
	void background_rect(ui::Screen scr, float x, float y, float z, float w, float h);
	/* Loads one SD image and cover-crops it for both screens. Empty clears it. */
	bool set_user_background(const std::string& path);
	bool user_background_loaded();

	/* gets the width of a screen */
	constexpr inline float screen_width(ui::Screen scr)
	{ return scr == ui::Screen::top ? ui::dimensions::width_top : ui::dimensions::width_bottom; }
	/* gets the height of the screen (exists for consistency) */
	constexpr inline float screen_height()
	{ return ui::dimensions::height; }

	/* used internally by set_x, set_y,
	 * returns the same coord unless it fits into
	 * ui::layer or ui::layout */
	float transform(BaseWidget *wid, float v);

	/* get the x position right from `from' for `newel' */
	float right(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* get the x position left from `from' for `newel' */
	float left(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* get the y position under `from' for `newel' */
	float under(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* get the y position above `from' for `newel' */
	float above(BaseWidget *from, BaseWidget *newel, float pad = 3.0f);
	/* Gets the center y position for owid in wid */
	float ycenter_rel(ui::BaseWidget *wid, ui::BaseWidget *owid);

	/* centers both `first' and `second' */
	void set_double_center(BaseWidget *first, BaseWidget *second, float pad = 3.0f);
	/* returns the center y position of `newel' from `from'. calling with newel > from is UB */
	float center_align_y(BaseWidget *from, BaseWidget *newel);

	/* global initializiation */
	bool init();
	/* init with render targets */
	void init(C3D_RenderTarget *top, C3D_RenderTarget *bot);
	/* global deinitialization */
	void exit();

	/* set the status line */
	void set_ticker(const std::string& text);
	void set_status(const std::string& text);
	void reset_status();
	bool status_running();
	u8 make_status_line_clear();
	void restore_status_line(u8);

	/* hides the bottom button bar and disables the konami key combo. returns whether this was enabled previously */
	bool set_focus(bool focus, bool with_action = true);


	/* sets the action description and returns the old one */
	prev_desc set_desc(const std::string& nlabel);
	prev_desc set_desc(str::type strid);
	prev_desc set_desc(prev_desc& prev);

	/* [msg]\nPress [A] to continue */
	void notice(const std::string& msg, float ypos = 70.0f);
	void notice(str::type msgid, float ypos = 70.0f);

	Result shell_is_open(bool *is_open);

	constexpr inline bool is_touched(const ui::Keys& keys)
	{
		return (keys.kDown | keys.kHeld) & KEY_TOUCH;
	}

	/* class for basic operations on an array of widgets */
	class WidgetGroup
	{
	public:
		void position_under(ui::BaseWidget *other, float initpad = 4.0f, float elpadding = 2.0f);
		void position_under_horizontal(ui::BaseWidget *other, float pad = 3.0f);
		void set_y_descending(float base, float elpadding = 2.0f);
		void translate(float x, float y);
		void add(ui::BaseWidget *w);
		void set_hidden(bool b);

		ui::BaseWidget *max_height();
		ui::BaseWidget *max_width();
		ui::BaseWidget *min_height();
		ui::BaseWidget *min_width();
		ui::BaseWidget *highest();
		ui::BaseWidget *lowest();
		ui::BaseWidget *rightmost();
		ui::BaseWidget *leftmost();

		inline ui::BaseWidget *back() { return this->size() == 0 ? nullptr : this->ws[this->size() - 1]; }
		inline std::vector<ui::BaseWidget *>& widgets() { return this->ws; }
		inline size_t size() { return this->ws.size(); }

		/* Calls render(ui::Keys&) on all widgets that render on screen. Returns false if
		 * one of the widgets also returns false. */
		bool render_all(ui::Screen, ui::Keys&);
		/* Calls render(ui::Keys&) on all widgets. If the group contains a widget that does
		 * not render on the current screen the render will break. Returns false if one
		 * of the widgets also returns false. */
		bool render_all(ui::Keys&);
		/* Calls destroy() on all widgets and deletes the memory. */
		void destroy_all();

	private:
		std::vector<ui::BaseWidget *> ws;

	};


	/* Renders a list of derivatives of ui::BaseWidget */
	class RenderQueue
	{
	public:
		~RenderQueue();

		/* ensure all RenderQueue's will stop rendering on all threads, if this is called
		 * this function may deadlock, be careful */
		static void terminate_render();

		/* push a new widget into the queue */
		void push(ui::BaseWidget *wid);
		/* Remove a widget from the queue. Musn't be called while rendering a frame. */
		void remove(ui::BaseWidget *wid);
		/* render forever */
		void render_forever();
		/* render until render_frame() returns false */
		void render_finite();
		/* Renders until keys.kDown & kDownMask > 0 */
		void render_until_button(u32 kDownMask);
		/* Renders until keys.kDown & kDownMask > 0 or if render_frame() returns false */
		void render_finite_button(u32 kDownMask);
		/* returns false if this should be the last frame,
		 * else returns true.
		 * Same as render_frame() except the global queue doesn't get rendered */
		bool render_exclusive_frame(ui::Keys&);
		/* returns false if this should be the last frame,
		 * else returns true */
		bool render_frame(ui::Keys&);
		/* returns false if this should be the last frame,
		 * else returns true */
		bool render_frame();
		/* renders only the bottom widgets */
		bool render_bottom(ui::Keys&);
		/* renders only the top widgets */
		bool render_top(ui::Keys&);
		/* renders a frame on `screen` */
		bool render_screen(ui::Keys&, ui::Screen screen);
		/* removes all widgets in the queue */
		void clear();

		/* runs the callback before the frame render begins. Runs only once
		 * NOTE: you can only have 1 callback every frame
		 * NOTE 2: only works on the global renderqueue */
		void before_render(std::function<void()> cb);

		/* runs the callback after the frame render is done. Runs only once
		 * NOTE: you can only have 1 callback every frame
		 * NOTE 2: only works on the global renderqueue */
		void render_and_then(std::function<bool()> cb);
		void render_and_then(std::function<void()> cb);
		/* Detaches a callback set by
		 * render_and_then */
		void detach_after();
		/* Signals the RenderQueue */
		void signal(u8 bits);
		/* Unsets a signal from the RenderQueue */
		void unsignal(u8 bits);
		/* find a widget by tag
		 * First searches top widgets, then bottom
		 * Returns nullptr if no matches were found */
		template <typename TWidget = ui::BaseWidget, typename TBase = ui::BaseWidget>
		TWidget *find_tag(int t)
		{
			for(TBase *w : this->top)
				if(w->matches_tag(t)) return (TWidget *) w;
			for(TBase *w : this->bot)
				if(w->matches_tag(t)) return (TWidget *) w;
			return nullptr;
		}
		/* gets the last pushed element
		 * returns nullptr if the queue is empty */
		template <typename TWidget = ui::BaseWidget>
		TWidget *back()
		{
			return (TWidget *) this->backPtr;
		}
		/* adds the last pushed element to a group */
		void group_last(ui::WidgetGroup& group)
		{
			group.add(this->backPtr);
		}

		void trigger_i18n_update(lang::type nlang);

		/* This only has effect on the global render queue */
		void set_exclusive_input(bool e) { this->exclusiveInput = e; }
		bool is_exclusive_input() { return this->exclusiveInput; }


		/* Gets the global RenderQueue */
		static ui::RenderQueue *global();

		static std::list<ui::RenderQueue *>& get_i18n_register();

		/* Gets the pressed keys */
		static ui::Keys get_keys();


		enum signal { signal_cancel = 1 };

		std::list<ui::BaseWidget *> sleepProcessors;
		std::list<ui::BaseWidget *> top;
		std::list<ui::BaseWidget *> bot;

	protected:
		bool render_list(std::list<ui::BaseWidget *>&, ui::Keys&);

		int enter_frame();
		bool exit_frame();

		std::function<bool()> *after_render_complete = nullptr;
		std::function<void()> *before_render_begin = nullptr;
		ui::BaseWidget *backPtr = nullptr;
		bool exclusiveInput = false;
		u8 signalBit = 0;


	};

	class I18NEnabledRenderQueue : public RenderQueue {
	public:
		I18NEnabledRenderQueue();
		~I18NEnabledRenderQueue();
	};

	static void trigger_i18n_update(lang::type nlang)
	{
		ui::RenderQueue::global()->trigger_i18n_update(nlang);

		for (ui::RenderQueue *q : ui::RenderQueue::get_i18n_register()) {
			q->trigger_i18n_update(nlang);
		}
	}

	namespace detail
	{
		inline constexpr float diff(float a, float b)
		{
			return a > b ? a - b : b - a;
		}

		/* builder for a ui::BaseWidget derivative */

		template <typename TWidget, typename TRV, typename TBase = ui::BaseWidget>
		class builder
		{
		public:
			using builder_i18n_update_cb_t = std::function<void(TWidget *, lang::type)>;

			template<typename ... Ts>
			builder(ui::Screen scr, Ts&& ... args)
			{
				this->el = new TWidget(scr);
				this->el->setup(args...);
			}

			builder(TWidget *nel) : el(nel), leaked(true) { }

			~builder()
			{
				if(!this->leaked && this->el != nullptr)
					delete this->el;
			}

			/* move constructor, used when you want to return a ui::builder in a function */
			builder(builder&& other)
				: el(other.el), leaked(other.leaked)
			{
				other.el = nullptr;
			}

			/* Sets the size of a widget. Not supported by all widgets */
			TRV& size(float x, float y) { this->el->resize(x, y); return this->return_value(); }
			/* Sets the size of a widget. Not supported by all widgets */
			TRV& size(float xy) { this->el->resize(xy, xy); return this->return_value(); }
			/* Sets the size of any potential widget children. Not supported by all widgets */
			TRV& size_children(float x, float y) { this->el->resize_children(x, y); return this->return_value(); }
			/* Sets the size of any potential widget children. Not supported by all widgets */
			TRV& size_children(float xy) { this->el->resize_children(xy, xy); return this->return_value(); }
			/* Autowraps the widget. Not supported by all widgets */
			TRV& wrap() { this->el->autowrap(); return this->return_value(); }
			/* Sets a border around the widget. Not supported by all widgets */
			TRV& border() { this->el->set_border(true); return this->return_value(); }
			/* Makes the widget scroll. Not supported by all widgets */
			TRV& scroll() { this->el->scroll(); return this->return_value(); }
			/* Do a manual configuration with a callback */
			TRV& configure(std::function<void(TWidget*)> conf) { conf(this->el); return this->return_value(); }
			/* Hide/unhide the widget */
			TRV& hide(bool hidden = true) { this->el->set_hidden(hidden); return this->return_value(); }
			/* Set the tag of the widget */
			TRV& tag(int t) { this->el->set_tag(t); return this->return_value(); }
			/* Set maximum width of the widget */
			TRV& max_width(float v) { this->el->set_max_width(v); return this->return_value(); }
			/* Set maximum height of the widget */
			TRV& max_height(float v) { this->el->set_max_height(v); return this->return_value(); }
			/* Set x position. note: you most likely want to call this last */
			TRV& x(float v) { this->el->set_x(v); return this->return_value(); }
			/* Set y position. note: you most likely want to call this last */
			TRV& y(float v) { this->el->set_y(v); return this->return_value(); }
			/* Set z position. note: you most likely want to call this last */
			TRV& z(float v) { this->el->set_z(v); return this->return_value(); }
			/* positions the widget under another widget */
			TRV& under(TBase *w, float pad = 3.0f) { this->el->set_y(ui::under(w, this->el, pad)); return this->return_value(); }
			/* positions the widget above another widget */
			TRV& above(TBase *w, float pad = 3.0f) { this->el->set_y(ui::above(w, this->el, pad)); return this->return_value(); }
			/* positions the widget right from another widget */
			TRV& right(TBase *w, float pad = 3.0f) { this->el->set_x(ui::right(w, this->el, pad)); return this->return_value(); }
			/* positions the widget in the center next to another widget */
			TRV& next_center(TBase *w, float pad = 3.0f) { ui::set_double_center(w, this->el, pad); return this->return_value(); }
			/* positions the widget left from another widget */
			TRV& left(TBase *w, float pad = 3.0f) { this->el->set_x(ui::left(w, this->el, pad)); return this->return_value(); }
			/* sets the y position to the center of w relative to the y-axis */
			TRV& middle(TBase *w, float offset = -1.0f) { this->el->set_y(ui::ycenter_rel(w, this->el) - offset); return this->return_value(); }
			/* sets the y position of the OTHER widget to the center of this relative to the y-axis */
			TRV& omiddle(TBase *w, float offset = -1.0f) { w->set_y(ui::ycenter_rel(this->el, w) - offset); return this->return_value(); }
			/* sets the y of the building widget to that of another one offseted so it hits the baseline of the other one */
			TRV& align_y(TBase *w, float offset = 0.0f) { this->el->set_y(w->get_y() - offset + diff(w->height(), this->el->height())); return this->return_value(); }
			/* sets the x of the building widget to that of another one */
			TRV& align_x(TBase *w, float offset = 0.0f) { this->el->set_x(w->get_x() + offset); return this->return_value(); }
			/* sets the y of the building widget to the center of another one */
			TRV& align_y_center(TBase *w) { this->el->set_y(ui::center_align_y(w, this->el)); return this->return_value(); }
			/* swaps the slots for a widget */
			TRV& swap_slots(const StaticSlot& slot) { this->el->swap_slots(slot); return this->return_value(); }
			/* marks the widget for exclusive input */
			TRV& exclusive_input() { this->el->set_exclusive_input(true); return this->return_value(); }

			/* installs a manual i18n update callback for more complicated update logic */
			TRV& manual_i18n_update(builder_i18n_update_cb_t cb)
			{
				TWidget *wid = this->el;
				this->el->i18n_manual_update_setup([wid, cb](lang::type nlang) -> void {
					cb(wid, nlang);
				});
				return this->return_value();
			}

			TRV& additional_i18n_update(builder_i18n_update_cb_t cb)
			{
				TWidget *wid = this->el;
				this->el->i18n_additional_update_setup([wid, cb](lang::type nlang) -> void {
					cb(wid, nlang);
				});
				return this->return_value();
			}

			/* Add the built widget to an I18NEnabledRenderQueue */
			void add_to(ui::I18NEnabledRenderQueue& queue) { queue.push((TBase *) this->finalize()); }
			/* Add the built widget to a RenderQueue */
			void add_to(ui::RenderQueue& queue) { queue.push((TBase *) this->finalize()); }
			/* Add the built widget to a RenderQueue */
			void add_to(ui::RenderQueue *queue) { queue->push((TBase *) this->finalize()); }
			/* Add the built widget to an I18NEnabledRenderQueue and your own pointer */
			void add_to(TWidget **ret, ui::I18NEnabledRenderQueue& queue) { *ret = this->finalize(); queue.push((TBase *) *ret); }
			/* Add the built widget to a RenderQueue and your own pointer */
			void add_to(TWidget **ret, ui::RenderQueue& queue) { *ret = this->finalize(); queue.push((TBase *) *ret); }
			/* Add the built widget to a RenderQueue and your own pointer */
			void add_to(TWidget **ret, ui::RenderQueue *queue) { *ret = this->finalize(); queue->push((TBase *) *ret); }
			/* finalize the built widget and return a pointer to it */
			TWidget *finalize() { this->el->finalize(); TWidget *ret = this->el; this->el = nullptr; return ret; }

			/* Add the built widget to a Group */
			template <typename T> void add_to(T& groupish) { groupish.add((TBase *) this->finalize()); }
			/* Add the built widget to a Group */
			template <typename T> void add_to(T *groupish) { groupish->add((TBase *) this->finalize()); }
			/* Add the built widget to a Group and your own pointer */
			template <typename T> void add_to(TWidget **ret, T& groupish) { *ret = this->finalize(); groupish.add((TBase *) *ret); }
			/* Add the built widget to a Group and your own pointer */
			template <typename T> void add_to(TWidget **ret, T *groupish) { *ret = this->finalize(); groupish->add((TBase *) *ret); }

		protected:
			virtual TRV& return_value() = 0;
			TWidget *el = nullptr;
			bool leaked = false;

		private:
			/* builders shouldn't be copied */
			builder operator = (const builder& other) = delete;

		};
	}

	/* base widget class */
	class BaseWidget
	{
		friend class RenderQueue;
	public:
		using i18n_update_cb_t = std::function<void(lang::type)>;
		template <typename TWidget>
		class Internal__MyBuilder : public ui::detail::builder<TWidget, Internal__MyBuilder<TWidget>>
		{
		public:
			using ui::detail::builder<TWidget, Internal__MyBuilder<TWidget>>::builder;
		protected:
			Internal__MyBuilder& return_value() { return *this; }
		};

	public:
		BaseWidget(ui::Screen scr)
			: screen(scr) { }

		/* not supposed to be overriden */
		virtual ~BaseWidget() = default;

		/* a simple basic constructor */
		virtual void setup() { }

		virtual void set_x(float x)
		{
			if(x == ui::layout::center_x) this->set_center_x();
			else this->x = ui::transform(this, x);
		}

		virtual void set_max_height(float) { }
		virtual void set_max_width(float) { }

		void set_raw_x(float x) { this->x = x; }
		void set_raw_y(float y) { this->y = y; }

		virtual void set_y(float y) { this->y = ui::transform(this, y); }
		virtual void set_z(float z) { this->z = z; }

		ui::Screen renders_on() { return this->screen; }

		virtual void set_center_x() { this->x = ui::transform(this, ui::layout::center_x); }

		virtual bool render(ui::Keys& keys) = 0;
		virtual void destroy()
		{
			if (this->does_additional_i18n_update && this->i18n_additional_cb != nullptr)
				delete this->i18n_additional_cb;
		}

		virtual float height() = 0;
		virtual float width() = 0;

		virtual const char *get_id() = 0;

		virtual float get_x() { return this->x; }
		virtual float get_y() { return this->y; }
		virtual float get_z() { return this->z; }

		virtual void finalize() { }

		bool set_hidden(bool b)
		{
			bool prev = this->hidden;
			this->hidden = b;
			return prev;
		}
		bool is_hidden() { return this->hidden; }

		bool i18n_update_setup() { return this->does_i18n_update; }
		bool i18n_additional_update_setup() { return this->does_additional_i18n_update; }

		void disable_i18n_update()
		{
			this->does_i18n_update = false;
		}

		virtual void i18n_auto_update_setup()
		{
			panic("setup_i18n_update(void) called on widget that does not implement it");
		}

		void i18n_manual_update_setup(i18n_update_cb_t cb)
		{
			this->i18n_update_cb = cb;
			this->does_i18n_update = true;
		}

		void i18n_additional_update_setup(i18n_update_cb_t cb)
		{
			if (!this->does_i18n_update)
				panic("attempting to install additional i18n update callback when widget is not set up for i18n update");

			if (this->does_additional_i18n_update && this->i18n_additional_cb != nullptr)
				delete this->i18n_additional_cb;

			this->i18n_additional_cb = new i18n_update_cb_t(cb);
			this->does_additional_i18n_update = true;
		}

		/* needed for sub widgets */
		void i18n_update(lang::type nlang)
		{
			if (!this->does_i18n_update)
				panic("i18n_update() called on widget that doesn't use it");

			this->i18n_update_cb(nlang);

			if (this->does_additional_i18n_update && this->i18n_additional_cb != nullptr)
				(*this->i18n_additional_cb)(nlang);
		}

		bool matches_tag(int t) { return this->tag == t; }
		void set_tag(int t) { this->tag = t; }

		virtual bool supports_theme_hook() { return false; }
		virtual void update_theme_hook() { }

		virtual void swap_slots(const StaticSlot& slot)
		{
			(void) slot;
			panic("swap_slots() called on widget that does not implement it");
		}

		virtual bool processes_in_sleep() { return false; }
		virtual bool process_in_sleep() { return true; }

		void set_exclusive_input(bool e) { this->exclusiveInput = e; }
		bool has_exclusive_input() { return this->exclusiveInput; }


	protected:
		ui::Screen screen;
		float z = ui::layer::middle;
		bool exclusiveInput = false;
		bool hidden = false;
		bool does_i18n_update = false;
		bool does_additional_i18n_update = false;
		i18n_update_cb_t i18n_update_cb;
		i18n_update_cb_t *i18n_additional_cb = nullptr;
		float x = 0, y = 0;
		int tag = 0;


	};

	template <typename T>
	using builder = typename T::template Internal__MyBuilder<T>;

	class SpriteStore
	{
	public:
		SpriteStore(const std::string& fname);
		SpriteStore() { }
		~SpriteStore();

		void open(const std::string& fname);
		size_t size();

		static C2D_Sprite get_by_id(ui::sprite id);


	private:
		C2D_SpriteSheet sheet = nullptr;


	};

	/* A wrapper for ui::BaseWidget with automatic:tm: management */
	template <typename TWidget>
	class ScopedWidget
	{
	public:
		~ScopedWidget()
		{
			this->destroy();
		}

		void destroy()
		{
			if(this->wid != nullptr)
			{
				this->wid->destroy();
				delete this->wid;
				this->wid = nullptr;
			}
		}

		template <typename ... Ts>
		void setup(ui::Screen scr, Ts&& ... args)
		{
			this->wid = new TWidget(scr);
			this->wid->setup(args...);
		}

		/* for some widgets it's of great importance to call this when
		 * you're finished configuring, you should always do it just in case */
		void finalize()
		{
			this->wid->finalize();
		}

		TWidget *ptr() { return this->wid; }

		TWidget *operator -> () { return this->wid; }
		TWidget& operator * () { return *this->wid; }


	private:
		TWidget *wid = nullptr;


	};

	UI_SLOTS_PROTO_EXTERN(Text_color)
	class Text : public ui::BaseWidget
	{ UI_WIDGET("Text")
	public:
		void setup(str::type labelid);
		void setup(const std::string& label);
		void setup() override; /* inits with an empty string */
		void destroy() override;

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void set_max_width(float w) override;
		void resize(float x, float y);
		void finalize() override;
		void autowrap();
		void scroll(); /* only supported with !center(), doesn't look amazing with multiline text but works */

		void set_center_x() override;

		/* sets the text of the widget to a new string */
		void set_text(str::type label);
		void set_text(const std::string& label);
		/* gets the current text of the widget */
		const std::string& get_text();
		/* gets the i18n str id if this text is registered for i18n updates */
		str::type get_i18n_strid();

		void i18n_auto_update_setup() override;

		void swap_slots(const StaticSlot&) override;

	private:
		UI_SLOTS_PROTO(Text_color, 1)

		void push_str(const std::string& str);
		void prepare_arrays();
		void reset_scroll();

		str::type label_strid = str::_i_max;
		float xsiz = 0.65f, ysiz = 0.65f;
		std::vector<C2D_Text> lines;
		C2D_TextBuf buf = nullptr;
		bool doAutowrap = false;
		float lineHeight = 0.0f;
		bool drawCenter = false;
		bool doScroll = false;
		float maxw = 0.0f;
		std::string text;

		struct ScrollCtx {
			size_t timing;
			size_t offset;
			float height;
			float width;
			float rx;
		} sctx;


	};

	class Sprite : public ui::BaseWidget
	{ UI_WIDGET("Sprite")
	public:
		void destroy() override { ui::ThemeManager::global()->unregister(this); }

		static void spritesheet(C2D_Sprite& sprite, u32 data)
		{
			C2D_DrawParams params = sprite.params;
			sprite = ui::SpriteStore::get_by_id((ui::sprite) data);
			params.pos.w = sprite.params.pos.w;
			params.pos.h = sprite.params.pos.h;
			sprite.params = params;
		}
		static void theme(C2D_Sprite& sprite, u32 data)
		{
			sprite.image = *ui::Theme::global()->get_image(data);
			if(sprite.image.subtex) /* this may be false for an optional image (and then has_image() would return false) */
			{
				sprite.params.pos.w = sprite.image.subtex->width;
				sprite.params.pos.h = sprite.image.subtex->height;
			}
		}
		static void image(C2D_Sprite& sprite, u32 data)
		{
			C2D_Image *ptr = (C2D_Image *) data;
			sprite.image = *ptr;
			sprite.params.pos.w = sprite.image.subtex->width;
			sprite.params.pos.h = sprite.image.subtex->height;
		}

		void setup(std::function<void(C2D_Sprite&, u32)> get_cb, u32 data = 0);

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void set_x(float x) override;
		void set_y(float y) override;
		void set_z(float z) override;

		void set_opacity(float value);

		void set_center(float x, float y);
		void set_data(u32 data);
		void rotate(float degs);

		inline C2D_Sprite& get_sprite() { return this->sprite; }

		bool supports_theme_hook() override { return true; }
		void update_theme_hook() override;

		inline bool has_image() { return this->sprite.image.tex != NULL; }
		inline C2D_Image *get_image() { return &this->sprite.image; }


	private:
		std::function<void(C2D_Sprite&, u32)> get_sprite_func;
		C2D_Sprite sprite;
		C2D_ImageTint tint;
		u32 unspecified_data;


	};

	UI_SLOTS_PROTO_EXTERN(Button_colors)
	class Button : public ui::BaseWidget
	{ UI_WIDGET("Button")
	public:
		using click_cb_t = std::function<bool(ui::Button *)>;

		void setup(std::function<void(C2D_Sprite&, u32)> get_image_cb, u32 data);
		void setup(const std::string& label);
		void setup(str::type i18n_strid);
		void destroy() override;
		void setup() override;

		template<typename T = ui::BaseWidget>
		inline T * const sub_widget() { return (T *)this->widget; }

		ui::BaseWidget * const get_widget() { return this->widget; }

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void set_border(bool b);

		void resize_children(float x, float y);
		void resize(float x, float y);
		void set_x(float x) override;
		void set_y(float x) override;
		void set_z(float z) override;

		void set_enabled(bool enabled);

		void i18n_auto_update_setup() override;

		bool press();

		/* autowrap for text size */
		void autowrap();
		/* update the label or set one */
		void set_label(const std::string& v);
		void set_label(str::type strid);
		/* get the label, if the subwidget is not a ui::Text this probably crashes */
		const std::string& get_label();

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue when_clicked(click_cb_t cb)
			{
				this->instance().on_click = cb;
				return this->return_value();
			}

			ReturnValue disable_background()
			{
				this->instance().showBg = false;
				return this->return_value();
			}
		};

		float textwidth();

		/* NOTE: This doesn't swap this->slots, but this->widget->slots...
		 * TODO: There should probably be a way to swap this->slots */
		void swap_slots(const StaticSlot&) override;

	private:
		UI_SLOTS_PROTO(Button_colors, 2)

		click_cb_t on_click = [](ui::Button *) -> bool { return true; };
		bool showBg = true, showBorder = false;
		ui::BaseWidget *widget = nullptr;
		float ox = 0.0f, oy = 0.0f;
		float w = 0.0f, h = 0.0f;
		str::type i18n_strid = str::_i_max;
		bool enabled;
		bool does_autowrap;

		void readjust();


	};

	/* utility */

	class ButtonCallback : public ui::BaseWidget
	{ UI_WIDGET("ButtonCallback")
	public:
		void setup(u32 keys);

		bool render(ui::Keys&) override;
		float height() override { return 0.0f; }
		float width() override { return 0.0f; }

		enum class ListenerType {
			down,
			held,
			up,
		};

		using on_activate_type = std::function<bool(u32)>;

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue when_kdown(on_activate_type cb)
			{
				this->instance().cb_down = cb;
				return this->return_value();
			}

			ReturnValue when_kheld(on_activate_type cb)
			{
				this->instance().cb_held = cb;
				return this->return_value();
			}

			ReturnValue when_kup(on_activate_type cb)
			{
				this->instance().cb_up = cb;
				return this->return_value();
			}
		};

	private:
		on_activate_type cb_down = [](u32) -> bool { return true; };
		on_activate_type cb_held = [](u32) -> bool { return true; };
		on_activate_type cb_up   = [](u32) -> bool { return true; };
		u32 keys;


	};

	UI_SLOTS_PROTO_EXTERN(Toggle_color)
	class Toggle : public ui::BaseWidget
	{ UI_WIDGET("Toggle")
	public:
		void setup(bool state, std::function<void()> on_toggle_cb);
		bool render(ui::Keys& keys) override;
		float height() override { return 20.0f; }
		float width() override { return 40.0f; }
		void toggle(bool toggled);
		void set_toggled(bool toggled);

	private:
		UI_SLOTS_PROTO(Toggle_color, 3)
		std::function<void()> toggle_cb;
		bool toggled_state;
		u64 last_touch_toggle;

	};

	namespace detail
	{
		class BackgroundObscurer : public ui::BaseWidget
		{ UI_WIDGET("BackgroundObscurer")
		public:
			void setup(float opacity = 0.60f);

			float height() override { return 0.0f; }
			float width() override { return 0.0f; }
			bool render(ui::Keys&) override;

			void set_opacity(float);

		private:
			u32 colour;

		};
	}

	bool is_exclusive_input();
	bool is_obscured();

	void exclusive_input(bool = true);
	void obscure(bool = true);

	template <typename T>
	std::string floating_prec(T inte, int prec = 2)
	{
		std::ostringstream ss; ss.precision(prec);
		ss << std::fixed << inte; return ss.str();
	}

	template <typename TInt>
	std::string human_readable_size(TInt i)
	{
		// Sorry for this mess.....
		if(i < 1024) return std::to_string(i) + " B"; /* < 1 KiB */
		if(i < 1024 * 1024) /* < 1 MiB */
			return floating_prec<float>((float) i / 1024) + " KiB";
		if(i < 1024 * 1024 * 1024) /* < 1 GiB */
			return floating_prec<float>((float) i / 1024 / 1024) + " MiB";
		if(i < (long long) 1024 * 1024 * 1024 * 1024) /* < 1TiB */
			return floating_prec<float>((float) i / 1024 / 1024 / 1024) + " GiB";
		return floating_prec<float>((float) i / 1024 / 1024 / 1024 / 1024) + " TiB";
	}

	template <typename TInt>
	std::string human_readable_size_block(TInt i)
	{
		std::string ret = human_readable_size<TInt>(i);
		TInt blk = (double) i / 1024 / 128;
		blk = blk == 0 ? 1 : blk;

		return PSTRING(size_plus_block, ret, blk);
	}
}

#endif
