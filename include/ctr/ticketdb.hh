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
#ifndef _inc_ctr_ticketdb_hh
#define _inc_ctr_ticketdb_hh

#include <vfs/diff.hh>
#include <irdb.hh>

#include <functional>

namespace ctr
{
	class TicketDatabase {
		struct PreHeader {
			u32 magic;
			u32 unk0;
			u32 unk1;
			u32 unk2;
		};

	public:
		TicketDatabase(ctr::FileHandle raw_tickdb) :
			raw_diff(std::move(raw_tickdb)),
			diff(raw_diff),
			irdb_section(diff, sizeof(PreHeader)),
			irdb(irdb_section) { }

		Result Initialize();

		Result FindTicket(vfs::IRDBFileEntry &entry, u64 title_id);
		Result ForEachTicket(std::function<Result(const vfs::IRDBFileEntry &, vfs::IRDBFile &)> callback);
		
		vfs::IRDBFile OpenTicket(vfs::IRDBFileEntry &ent);

		void DetachFile(ctr::FileHandle &target) {
			return raw_diff.DetachFile(target);
		}

	private:
		vfs::FileBase raw_diff; // raw file on nand
		vfs::DIFFReader diff; // inner contents of diff
		vfs::SubFile irdb_section; // irdb section inside diff
		vfs::IRDBReader irdb; // irdb driver
	};
};

#endif