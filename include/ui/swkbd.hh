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

#ifndef inc_ui_swkbd_hh
#define inc_ui_swkbd_hh

#include <ui/base.hh>
#include <3ds.h>

#include <functional>
#include <string>

namespace ui
{
	class AppletSwkbd : public ui::BaseWidget
	{ UI_WIDGET("AppletSwkbd")
	public:
		void setup(std::string *ret, int maxLen = 200, SwkbdType type = SWKBD_TYPE_NORMAL,
			int numBtns = 2);

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue button(SwkbdButton *b)
			{
				this->instance().buttonPtr = b;
				return this->return_value();
			}

			ReturnValue result(SwkbdResult *r)
			{
				this->instance().resPtr = r;
				return this->return_value();
			}
		};

		/* set the hint text, the text displayed transparantly
		 * if you don't have any text entered */
		void hint(const std::string& h);
		/* Sets the password mode, see enum SwkbdPasswordMode for possible values */
		void passmode(SwkbdPasswordMode mode);
		/* Sets the initial search text */
		void init_text(const std::string& t);
		/* Sets the valid input mode,
		 * filterflags is a bitfield of SWKBD_FILTER_* */
		void valid(SwkbdValidInput mode, u32 filterFlags = 0, u32 maxDigits = 0);


	private:
		SwkbdButton *buttonPtr = nullptr;
		SwkbdResult *resPtr = nullptr;
		SwkbdState state;
		std::string *ret;
		size_t len;


	};

	class KBDEnabledButton : public ui::BaseWidget
	{ UI_WIDGET("KBDEnabledButton")
	public:
		using update_callback_type = std::function<void(KBDEnabledButton *)>;

		void setup(str::type empty_strid, str::type hint_strid, size_t min_len = 0);
		void setup(const std::string& empty_label, const std::string& hint, size_t min_len = 0);

		bool render(ui::Keys&) override;

		float height() override { return this->btn->height(); }
		float width() override { return this->btn->width(); }

		void set_x(float x) override { this->btn->set_x(x); }
		void set_y(float y) override { this->btn->set_y(y); }

		float get_x() override { return this->btn->get_x(); }
		float get_y() override { return this->btn->get_y(); }

		void i18n_auto_update_setup() override;

		void press();

		void set_text_value(const std::string &val);
		void update();

		void set_numpad_mode(bool enable);

		const std::string& value();
		uint64_t numpad_value();

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue when_update(update_callback_type cb)
			{
				this->instance().update_cb = cb;
				return this->return_value();
			}

			ReturnValue numpad(bool enable)
			{
				this->instance().set_numpad(enable);
				return this->return_value();
			}
		};

		bool is_empty() { return !this->hasValue; }

	private:
		update_callback_type update_cb = nullptr;
		str::type empty_strid, hint_strid;
		ui::ScopedWidget<ui::Button> btn;
		bool hasValue, doNumpad = false;
		uint64_t numpadResult = 0;
		std::string empty, hint;
		std::string text_value;
		size_t min_len;

	};

	/* Gets string input with ui::AppletSwkbd */
	std::string keyboard(std::function<void(ui::AppletSwkbd *)> configure,
		SwkbdButton *btn = nullptr, SwkbdResult *res = nullptr, size_t length = 200);
	/* Gets a number with ui::AppletSwkbd */
	uint64_t numpad(std::function<void(ui::AppletSwkbd *)> configure,
		SwkbdButton *btn = nullptr, SwkbdResult *res = nullptr, size_t length = 10);

/*	class Swkbd
	{

	};*/
}

#endif

