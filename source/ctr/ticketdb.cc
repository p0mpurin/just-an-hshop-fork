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
#include <ticketdb.hh>
#include <vfs/util.hh>

Result ctr::TicketDatabase::Initialize()
{
	Result res = 0;

	VTRY(raw_diff.Initialize());

	VTRY(diff.Initialize());

	if (diff.GetSize() < sizeof(PreHeader))
		return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 6);

	PreHeader pre_header;

	VTRY(diff.Read(0, &pre_header, sizeof(PreHeader)));

	if (pre_header.magic != 0x4B434954)
		return MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 7);

	VTRY(irdb_section.Initialize());
	VTRY(irdb.Initialize());
	return 0;
}

Result ctr::TicketDatabase::FindTicket(vfs::IRDBFileEntry &entry, u64 title_id) {
	return irdb.FindFile(entry, title_id);
}

Result ctr::TicketDatabase::ForEachTicket(std::function<Result(const vfs::IRDBFileEntry &, vfs::IRDBFile &)> callback)
{
	auto [res, i] = irdb.Iterate();
	if (R_FAILED(res)) return res;

	do {
		vfs::IRDBFile f(irdb, **i);
		VTRY(callback(**i, f));
	} while (res = i->Next(), R_SUCCEEDED(res) && res != vfs::IRDBFileIterator::REACHED_END);

	if (R_FAILED(res))
		return res;

	return 0;
}

ctr::vfs::IRDBFile ctr::TicketDatabase::OpenTicket(ctr::vfs::IRDBFileEntry &ent) {
	return std::move(vfs::IRDBFile{irdb, ent});
}