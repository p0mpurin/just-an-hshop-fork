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

#include <ui/menuselect.hh>
#include "panic.hh"

#define MAX_PER_PAGE 8
#define MIN(a,b) ((a)>(b)?(b):(a))

static u32 menu_text_normal()
{ return *ui::Theme::global()->get_color(ui::theme::text_color); }
static u32 menu_text_focus()
{ return C2D_Color32(255, 220, 238, 255); }
UI_STATIC_SLOTS(menu_normal_slot, "MenuSelectTextNormal", menu_text_normal)
UI_STATIC_SLOTS(menu_focus_slot, "MenuSelectTextFocus", menu_text_focus)


void ui::MenuSelect::setup()
{
	this->w = ui::screen_width(this->screen) - 20.0f;
	this->h = 22.0f;

	constexpr float y = 14.0f + (22.0f + 2.0f) * MAX_PER_PAGE;
	this->hint.setup(this->screen, str::hint_navigate);
	this->hint->set_x(12.0f);
	this->hint->set_y(y);
	this->hint->resize(0.4f, 0.4f);
}

void ui::MenuSelect::select(size_t pos, u32 pressed_keys)
{
	this->i = pos;
	/* must be executed after this->i is set */
	if(this->cursor_move_callback)
		this->cursor_move_callback();
	this->kdown = pressed_keys;
}

#define PUSH_BUTTON(lbl) \
	u32 myi = this->btns.size(); \
	ui::Button *b = ui::builder<ui::Button>(this->screen, lbl) \
		.when_clicked([this, myi](ui::Button *) -> bool { \
			/* pressing equates to clicking A */ \
			this->select(myi, KEY_A); \
			this->call_current(); \
			return true; \
		}) \
		.size(this->w, this->h) \
		.disable_background() \
		.x(ui::layout::center_x) \
		.finalize(); \
	b->set_border(false); \
	if((myi % MAX_PER_PAGE) == 0) b->set_y(14.0f); \
	else                          b->set_y(ui::under(this->btns.back(), b, 2.0f)); \
	this->btns.push_back(b);

void ui::MenuSelect::push_button(const std::string& label)
{
	PUSH_BUTTON(label)
}

void ui::MenuSelect::push_button(str::type strid)
{
	PUSH_BUTTON(strid)
}

void ui::MenuSelect::add_row(const std::string& s, callback_t c)
{
	panic_assert(!this->main_callback, "attempt to add callback row with main callback enabled");
	this->funcs.push_back(c);
	this->push_button(s);
}

void ui::MenuSelect::add_row(const std::string& label)
{
	panic_assert(this->main_callback, "attempt to add callback-less row without main callback");
	this->push_button(label);
}

void ui::MenuSelect::add_row(str::type sid)
{
	panic_assert(this->main_callback, "attempt to add callback-less row without main callback");
	this->push_button(sid);
}

void ui::MenuSelect::add_row(str::type sid, callback_t c)
{
	panic_assert(!this->main_callback, "attempt to add callback row with main callback enabled");
	this->funcs.push_back(c);
	this->push_button(sid);
}

float ui::MenuSelect::height()
{
	size_t s = this->btns.size();
	if(s == 0) return 0.0f;
	return s * this->h + (s - 1) * 5.0f;
}

float ui::MenuSelect::width()
{
	return this->w;
}

bool ui::MenuSelect::render(ui::Keys& k)
{
	panic_assert(this->btns.size() != 0, "Empty menuselect");

#define MOVE(with) do { with; if(this->cursor_move_callback) this->cursor_move_callback(); } while(0)
	if((k.kDown & KEY_UP) && this->i > 0) MOVE(--this->i);
	if((k.kDown & KEY_DOWN) && this->i < this->btns.size() - 1) MOVE(++this->i);
	if(k.kDown & KEY_LEFT)
	{
		if(this->i >= MAX_PER_PAGE) MOVE(this->i -= MAX_PER_PAGE);
		else                        MOVE(this->i = 0);
	}
	if(k.kDown & KEY_RIGHT)
	{
		if(this->i + MAX_PER_PAGE < this->btns.size()) MOVE(this->i += MAX_PER_PAGE);
		else if(this->i < this->btns.size())           MOVE(this->i = this->btns.size() - 1);
	}
#undef MOVE
	if(k.kDown & this->kdownmask)
	{
		this->kdown = k.kDown;
		this->call_current();
	}

	/* aka u32 i = start_of_page */
	u32 start = this->i - (this->i % MAX_PER_PAGE);
	u32 end = MIN(start + MAX_PER_PAGE, this->btns.size());
	/* Dark panel backdrop — slightly richer opacity for better text contrast */
	C2D_DrawRectSolid(6.0f, 7.0f, -0.30f,
		ui::screen_width(this->screen) - 12.0f, 202.0f,
		C2D_Color32(0, 0, 0, 186));
	/* Subtle pink accent line along the top of the panel */
	C2D_DrawRectSolid(16.0f, 8.0f, -0.29f,
		ui::screen_width(this->screen) - 32.0f, 1.0f,
		C2D_Color32(255, 164, 204, 42));
	float target_y = 14.0f + (this->i - start) * 24.0f;
	this->highlight_y += (target_y - this->highlight_y) * 0.34f;
	/* Selection highlight — soft pink fill with more presence */
	C2D_DrawRectSolid(18.0f, this->highlight_y, -0.08f,
		ui::screen_width(this->screen) - 36.0f, this->h,
		C2D_Color32(255, 134, 188, 38));
	/* Left accent indicator — thicker and more visible, signals current selection */
	C2D_DrawRectSolid(18.0f, this->highlight_y + 3.0f, 0.0f,
		3.0f, this->h - 6.0f, C2D_Color32(255, 164, 204, 225));

	for(u32 i = start; i < end; ++i)
	{
		/* Buttons keep full-width touch targets, but their labels behave like
		 * a left-aligned command palette instead of stock centered buttons. */
		this->btns[i]->get_widget()->set_x(29.0f);
		this->btns[i]->swap_slots(i == this->i ? menu_focus_slot : menu_normal_slot);
		this->btns[i]->render(k);
	}
	this->hint->render(k);

	return true;
}

void ui::MenuSelect::i18n_auto_update_setup()
{
	this->i18n_update_cb = [this](lang::type nlang) -> void {
		for (ui::Button *btn : this->btns) {
			if (btn->i18n_update_setup())
				btn->i18n_update(nlang);
		}
	};
	this->does_i18n_update = true;
}

void ui::MenuSelect::clear()
{
	this->funcs.clear();
	this->btns.clear();
}

void ui::MenuSelect::call_current()
{
	ui::RenderQueue::global()->render_and_then((std::function<bool()>) [this]() -> bool {
		if(this->main_callback) return this->main_callback();
		else                    return this->funcs[this->i]();
	});
}
