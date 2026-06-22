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

#ifndef inc_ui_confirm_hh
#define inc_ui_confirm_hh

#include <ui/base.hh>

#include <string>


namespace ui
{
	class Confirm : public ui::BaseWidget
	{ UI_WIDGET("Confirm")
	public:
		/* does not add a label */
		void setup(bool& ret);
		void setup(const std::string& label, bool& ret);
		void setup(str::type labelid, bool& ret);

		bool render(ui::Keys&) override;
		float height() override;
		float width() override;

		void set_y(float y) override;

		void i18n_auto_update_setup() override;

		void set_label(const std::string& label);
		void set_label(str::type labelid);

		static bool exec(const std::string& label, const std::string& label_top = "", bool default_val = false);
		static bool exec(str::type labelid, str::type label_top_id = str::_i_max, bool default_val = false);


	private:
		ui::RenderQueue queue;
		str::type labelid;
		bool *ret;

		void adjust();


	};
}

#endif

