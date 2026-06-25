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
#ifndef _inc_ctr_mng_hh
#define _inc_ctr_mng_hh
 
#include <3ds.h>

#include <ctr/titleid.hh>
#include <vector>

namespace ctr::mng
{
	namespace dspfirm
	{
		static constexpr const char *default_path = "/3ds/dspfirm.cdc";

		Result check_existing(const char *path = default_path);
		Result dump(const char *path = default_path);

		Result ensure_auto(const char *path = default_path);
	};

	u8 get_system_region();
	bool is_n3ds();

	namespace DeleteTitleFlag
	{
		enum {
			None         = 0,
			DeleteTicket = 1,
			CheckExist   = 2,
		};
		using Type = u32;
	}

	Result list_titles_on(FS_MediaType media, std::vector<ctr::title_id>& ret);
	Result list_tickets(std::vector<ctr::title_id>& ret);

	Result get_free_space(InstallDestination dest, u64 *size);
	Result get_title_entry(ctr::title_id tid, AM_TitleEntry& entry);

	bool title_exists(ctr::title_id tid);
	bool title_exists_anywhere(ctr::title_id tid);
	bool ticket_exists(ctr::title_id tid);

	Result delete_title(ctr::title_id tid, DeleteTitleFlag::Type flags);
	Result delete_ticket(ctr::title_id tid);

	Result import_seed(ctr::title_id tid, const u8 (*seed)[16]);

	void shutdown_console();
};

#endif