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

#ifndef inc_ui_list_hh
#define inc_ui_list_hh

#include <functional>

#include <ui/base.hh>
#include <panic.hh>

namespace ui
{
	template <typename T>
	class List : public ui::BaseWidget
	{ UI_WIDGET("List")
	public:
		using on_select_type = std::function<bool(List<T> *, size_t, u32)>;
		using on_change_type = std::function<void(List<T> *, size_t)>;
		using to_string_type = std::function<std::string(const T&)>;

		static constexpr size_t button_timeout_held = 7;
		static constexpr float scrollbar_width = 3.0f;
		static constexpr float selector_offset = 3.0f;
		static constexpr float text_spacing = 17.0f;
		static constexpr size_t button_timeout = 11;
		static constexpr float text_offset = 15.0f;
		static constexpr float text_size = 0.65;

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue when_select(on_select_type cb)
			{
				this->instance().on_select_ = cb;
				return this->return_value();
			}

			ReturnValue when_change(on_change_type cb)
			{
				this->instance().on_change_ = cb;
				return this->return_value();
			}

			ReturnValue to_string(to_string_type cb)
			{
				this->instance().to_string_ = cb;
				return this->return_value();
			}

			ReturnValue buttons(u32 k)
			{
				this->instance().keys |= k;
				return this->return_value();
			}
		};

		void setup(std::vector<T> *items)
		{
			this->charsLeft = this->capacity = 1;
			this->buf = C2D_TextBufNew(1);
			this->items = items;

			this->buttonTimeout = 0;
			this->amountRows = 12;
			this->keys = KEY_A;
			this->view = 0;
			this->pos = 0;

			this->sx = ui::screen_width(this->screen) - List::scrollbar_width - 5.0f;

			static ui::slot_color_getter getters[] = {
				color_text, color_scrollbar, color_button, color_button_border
			};
			this->slots = ui::ThemeManager::global()->get_slots(this, "List", 4, getters);
		}

		void destroy() override
		{
			/* always allocated in setup() */
			C2D_TextBufDelete(this->buf);
		}

		void finalize() override { this->update(); }

		bool render(ui::Keys& keys) override
		{
			/* handle input */
			u32 effective = keys.kDown | keys.kHeld;

			if(this->buttonTimeout != 0)
			{
				--this->buttonTimeout;
				// please don't scream at me for using goto as a convenience
				goto builtin_controls_done;
			}

			if(effective & KEY_UP)
			{
				this->buttonTimeout = List::button_timeout;
				if(this->pos > 0)
				{
					if(this->pos == this->view)
						--this->view;
					--this->pos;
				}
				else
				{
					this->pos = this->lines.size() - 1;
					this->view = this->last_full_view();
				}
				this->on_change_(this, this->pos);
				this->update_scrolldata();
			}

			if(effective & KEY_DOWN)
			{
				this->buttonTimeout = List::button_timeout;
				size_t old = this->pos;
				if(this->pos < this->lines.size() - 1)
				{
					++this->pos;
					if(this->pos == this->view + this->amountRows - 1 && this->view < this->last_full_view())
						++this->view;
				}
				else
				{
					this->view = 0;
					this->pos = 0;
				}
				if(this->pos != old)
				{
					this->on_change_(this, this->pos);
					this->update_scrolldata();
				}
			}

			if(effective & KEY_LEFT)
			{
				this->buttonTimeout = List::button_timeout;
				this->view -= this->min<int>(this->amountRows, this->view);
				this->pos -= this->min<int>(this->amountRows, this->pos);
				this->on_change_(this, this->pos);
				this->update_scrolldata();
			}

			if(effective & KEY_RIGHT)
			{
				this->buttonTimeout = List::button_timeout;
				this->view = this->min<size_t>(this->view + this->amountRows - 1, this->last_full_view());
				this->pos = this->min<size_t>(this->pos + this->amountRows - 1, this->lines.size() - 1);
				this->on_change_(this, this->pos);
				this->update_scrolldata();
			}

			if(keys.kHeld & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT))
				this->buttonTimeout = List::button_timeout_held; /* the timeout is a bit less if we want to hold */

builtin_controls_done:

			/* Smoked glass catalogue plane. */
			C2D_DrawRectSolid(this->x, this->y - 4.0f, this->z - 0.30f,
				this->sx - this->x - 3.0f, this->height() + 5.0f,
				C2D_Color32(0, 0, 0, 178));
			/* A faint reflected edge suggests glass without boxing it in. */
			C2D_DrawRectSolid(this->x + 8.0f, this->y - 3.0f, this->z - 0.29f,
				this->sx - this->x - 19.0f, 1.0f,
				C2D_Color32(255, 255, 255, 24));

			/* render scrollbar */
			if(this->lines.size() > this->amountRows)
			{
				C2D_DrawRectSolid(this->sx, this->y - 4.0f, this->z - 0.20f,
					List::scrollbar_width, this->height() + 5.0f,
					C2D_Color32(255, 255, 255, 18));
				C2D_DrawRectSolid(this->sx, this->sy, this->z - 0.10f,
					List::scrollbar_width, this->sh,
					this->slots.get(1));
			}

			/* render the on-screen elements */
			size_t end = this->view + (this->lines.size() > this->amountRows
				? this->amountRows - 1 : this->lines.size());
			u32 color = this->slots.get(0);
			for(size_t i = this->view, j = 0; i < end; ++i, ++j)
			{
				float ofs = 0;

				if(i == this->pos)
				{
					/* update offsets */
					if(this->scrolldata.shouldScroll)
					{
						ofs = this->scrolldata.xof;
						if(this->scrolldata.framecounter <= 60)
							++this->scrolldata.framecounter;
						else if(ui::screen_width(this->screen) - this->x - List::text_offset + ofs > this->selw + 10.0f)
						{
							if(this->scrolldata.framecounter > 120)
							{
								this->scrolldata.framecounter = 0;
								this->scrolldata.xof = 0;
							}
							else ++this->scrolldata.framecounter;
						}
						else this->scrolldata.xof += 0.2f;
					}
				}

				u32 draw_color = i == this->pos
					? C2D_Color32(255, 220, 238, 255) : color;
				C2D_DrawText(&this->lines[i], C2D_WithColor, this->x + List::text_offset - ofs,
					this->y + List::text_spacing * j, this->z + 0.05f, List::text_size, List::text_size,
					draw_color);
			}

			if(keys.kDown & this->keys)
				return this->on_select_(this, this->pos, keys.kDown);
			return true;
		}

		float height() override
		{
			return this->lines.size() < this->amountRows
				? this->lines.size() * List::text_spacing
				: this->amountRows * List::text_spacing;
		}

		float width() override
		{
			return this->sx + List::scrollbar_width - this->x;
		}

		/* Rerenders all items in the list
		 * NOTE: if you simply want to update a certain item
		 *       consider using update(size_t)
		 **/
		void update()
		{
			this->clear();
			for(T& i : *this->items)
				this->append_text(i);
			this->update_scrolldata();
		}
		/* Amount of items ready to be rendered
		 **/
		size_t size() { return this->lines.size(); }

		/* Appends an item to items and appends it to the available texts to be rendered
		 **/
		void append(const T& val)
		{
			this->items->push_back(val);
			this->append_text(val);
		}

		/* Add a key that triggers select
		 **/
		void key(u32 k)
		{
			this->keys |= k;
		}

		/* Amount of items visible
		 **/
		size_t visible()
		{
			return this->lines.size() > this->amountRows
				? this->amountRows : this->lines.size();
		}

		/* Returns the element at i with range checking
		 **/
		T& at(size_t i)
		{
			return this->items->at(i);
		}

		/* Gets the current cursor position
		 **/
		size_t get_pos()
		{
			return this->pos;
		}

		/* Sets the current cursor position
		 **/
		void set_pos(size_t p)
		{
			if(p < this->lines.size())
			{
				this->view = this->min<size_t>(p > 2 ? p - 2 : 0, this->last_full_view());
				this->pos = p;
				this->update_scrolldata();
			}
		}


	private:
		on_select_type on_select_ = [](List<T> *, size_t, u32) -> bool { return true; };
		on_change_type on_change_ = [](List<T> *, size_t) -> void { };
		to_string_type to_string_ = [](const T&) -> std::string { return ""; };

		ui::SlotManager slots { nullptr };

		size_t charsLeft;
		size_t capacity;
		C2D_TextBuf buf;

		float selw; /* width of the selected text */
		float selh; /* height of the selected text */
		float sh; /* scrollbar height */
		float sx; /* scrollbar x */
		float sy; /* scrollbar y */

		std::vector<C2D_Text> lines;
		std::vector<T> *items;

		u32 keys; /* keys that trigger select */

		int buttonTimeout; /* amount of frames that need to pass before the next button can be pressed */
		size_t amountRows; /* amount of rows to be drawn */
		size_t view; /* first visible element */
		size_t pos; /* cursor position */

		struct ScrolldataContainer {
			bool shouldScroll;
			int framecounter;
			float xof;
		} scrolldata;

		template <typename TInt>
		TInt min(TInt a, TInt b)
		{
			return a > b ? b : a;
		}

		void append_text(const T& val)
		{
			std::string s = this->to_string_(val);
			this->alloc(s.size() + 1 /* +1 for NULL term */);
			this->lines.emplace_back();
			ui::parse_text(&this->lines.back(), this->buf, s.c_str());
			C2D_TextOptimize(&this->lines.back());
		}

		void update_scrolldata()
		{
			/* selw, selh */
			C2D_TextGetDimensions(&this->lines[this->pos], List::text_size, List::text_size,
				&this->selw, &this->selh);
			/* sy */
			this->sy = ((float) this->view / (float) this->lines.size()) * this->height()
				+ this->y;
			/* sh */
			this->sh = this->min<float>(
					ui::screen_height() - this->sy - this->y,
					((float) this->visible() / (float) this->lines.size()) * this->height()
				);

			this->scrolldata = {
				ui::screen_width(this->screen) - this->x - List::text_offset < this->selw,
				0, 0
			};
		}

		void clear()
		{
			this->charsLeft = this->capacity;
			C2D_TextBufClear(this->buf);
			this->lines.clear();
		}

		void alloc(size_t n)
		{
			if(this->charsLeft < n)
			{
				this->capacity += n * 2;
				this->buf = C2D_TextBufResize(this->buf, this->capacity);
				this->charsLeft += n * 2;
				/* we need to update all text items */
				for(C2D_Text& line : this->lines)
					line.buf = this->buf;
			}

			this->charsLeft -= n;
		}

		size_t last_full_view()
		{
			return this->lines.size() > this->amountRows
				? this->lines.size() - this->amountRows + 1 : 0;
		}

		UI_CTHEME_GETTER(color_scrollbar, ui::theme::scrollbar_color)
		UI_CTHEME_GETTER(color_text, ui::theme::text_color)
		UI_CTHEME_GETTER(color_button, ui::theme::button_background_color)
		UI_CTHEME_GETTER(color_button_border, ui::theme::button_border_color)


	};
}

#endif
