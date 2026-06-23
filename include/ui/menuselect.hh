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

#ifndef inc_ui_menuselect_hh
#define inc_ui_menuselect_hh

#include <ui/base.hh>
#include <functional>
#include <string>
#include <vector>


namespace ui
{
	class MenuSelect : public ui::BaseWidget
	{ UI_WIDGET("MenuSelect")
	public:
		using callback_t = std::function<bool()>;

		void setup() override;
		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue add_row(const std::string& s, callback_t c)
			{
				this->instance().add_row(s, c);
				return this->return_value();
			}

			ReturnValue add_row(str::type sid, callback_t c)
			{
				this->instance().add_row(sid, c);
				return this->return_value();
			}

			ReturnValue keymask(u32 k)
			{
				this->instance().kdownmask = k;
				return this->return_value();
			}

			ReturnValue when_select(callback_t cb)
			{
				this->instance().main_callback = cb;
				return this->return_value();
			}

			ReturnValue when_changed(callback_t cb)
			{
				this->instance().cursor_move_callback = cb;
				return this->return_value();
			}
		};

		void select(size_t pos, u32 pressed_keys);

		void add_row(const std::string& label, callback_t callback);
		void add_row(const std::string& label);
		void add_row(str::type label_strid, callback_t callback);
		void add_row(str::type label_strid);
		void clear();

		void set_pos(u32 i)
		{
			this->select(i, KEY_A);
		}
		u32 kdown_for_press() { return this->kdown; }
		u32 pos() { return this->i; }

		void i18n_auto_update_setup() override;


	private:
		void push_button(const std::string& label);
		void push_button(str::type strid);
		void call_current();

		ui::ScopedWidget<ui::Text> hint;

		callback_t cursor_move_callback = nullptr;
		callback_t main_callback = nullptr;
		std::vector<ui::Button *> btns;
		std::vector<callback_t> funcs;
		u32 kdownmask = KEY_A, kdown;
		float w, h;
		float highlight_y = 14.0f;
		u32 i = 0;


	};
}

#endif
