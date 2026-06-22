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
#ifndef _inc_ctr_wlan_hh
#define _inc_ctr_wlan_hh

#include <3ds.h>

namespace ctr::wlan
{
	Result connect(Handle disconnect_event, u64 timeout_ns = -1);
	Result disconnect();
	bool is_connected();

	Handle *connect_mtx();

	u8 strength();

	bool is_enabled();
	Result enable();
	Result disable();
}

#endif