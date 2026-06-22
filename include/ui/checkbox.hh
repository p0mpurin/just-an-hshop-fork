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

#ifndef inc_ui_checkbox_hh
#define inc_ui_checkbox_hh

#include <ui/base.hh>
#include <functional>


namespace ui
{
	UI_SLOTS_PROTO_EXTERN(CheckBox)
	class CheckBox : public ui::BaseWidget
	{ UI_WIDGET("CheckBox")
	public:
		void setup(bool isInitialChecked = false);

		void resize(float nw, float nh);

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void set_y(float y) override;
		void set_x(float x) override;

		bool checked() { return !!(this->flags & CHECKED); }

		UI_BUILDER_EXTENSIONS()
		{
			UI_USING_BUILDER()
		public:
			ReturnValue when_change(std::function<void(bool)> cb)
			{
				this->instance().on_change_cb = cb;
				return this->return_value();
			}
		};

		void set_checked(bool c);


	private:
		UI_SLOTS_PROTO(CheckBox, 2)
		std::function<void(bool)> on_change_cb = [](bool) -> void { };
		enum {
			CHECKED = 0x1,
		};
		float ox, oy;
		float w, h;
		u8 flags;

	};
}

#endif

