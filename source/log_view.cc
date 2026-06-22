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

#include "log_view.hh"
#include "hsapi.hh"
#include "i18n.hh"
#include "log.hh"

#include <ui/menuselect.hh>
#include <ui/loading.hh>
#include <ui/base.hh>
#include <string>


static bool upload_logs()
{
	log_flush();
	char *data = log_retrieve();
	std::string id;
	Result res;

	if(data)
	{
		res = hsapi::call(hsapi::upload_log, (const char *) data, (u32) strlen(data), id);
		free(data);
	}
	else res = APPERR_OUT_OF_MEM;

	if(R_FAILED(res))
	{
		error_container err = get_error(res);
		handle_error(err);
		return true;
	}
	ui::notice(PSTRING(log_id, id));
	return true;
}

static bool clear_logs()
{
	ui::loading([]() -> void {
		log_del();
	});
	return true;
}

void show_logs_menu()
{
	bool focus = ui::set_focus(true);

	ui::I18NEnabledRenderQueue queue;
	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.add_row(str::upload_logs, upload_logs)
		.add_row(str::clear_logs, clear_logs)
		.add_to(queue);

	queue.render_finite_button(KEY_B);

	ui::set_focus(focus);
}

