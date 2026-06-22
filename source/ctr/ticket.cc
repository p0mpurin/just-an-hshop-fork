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
#include <ctr/ticket.hh>

Result ctr::ticket::TicketBlob::Initialize() {
	BlobHeader blobhdr;

	if (file.GetSize() < sizeof(BlobHeader))
		return vfs::DataTooShort();

	Result res = 0;

	VTRY(file.Read(0, &blobhdr, sizeof(BlobHeader)));

	this->num_tickets = blobhdr.num_tickets;
	return res;
}

std::pair<Result, std::optional<ctr::vfs::SubFile>> ctr::ticket::TicketBlob::OpenTicket(u32 index) {
	Result res = 0;

	if (index > TicketCount() - 1)
		return std::pair(vfs::OutOfBounds(), std::nullopt);

	u32 cur_tik_start_offset = sizeof(BlobHeader);
	u32 cur_tik_size = 0;

	for (u32 tiks_read = 0; tiks_read != index + 1; tiks_read++) {
		cur_tik_start_offset += cur_tik_size;
		be_int<u32> table_size = 0;
		res = file.Read(cur_tik_start_offset + sizeof(StaticTicket) + 4, &table_size, sizeof(u32));
		if (R_FAILED(res))
			return std::pair(res, std::nullopt);
		cur_tik_size = sizeof(StaticTicket) + table_size;
	}

	ctr::vfs::SubFile sf(file, cur_tik_start_offset, cur_tik_size);
	res = sf.Initialize();
	if (R_FAILED(res))
		return std::pair(res, std::nullopt);

	return std::pair(res, std::move(sf));
}

std::pair<Result, std::optional<ctr::vfs::SubFile>> ctr::ticket::TicketBlob::OpenTicket(u64 ticket_id) {
	Result res = 0;

	u32 cur_tik_start_offset = sizeof(BlobHeader);
	u32 cur_tik_size = 0;
	bool found = false;

	for (u32 tiks_read = 0; tiks_read != TicketCount(); tiks_read++) {
		cur_tik_start_offset += cur_tik_size;
		be_int<u32> table_size = 0;
		res = file.Read(cur_tik_start_offset + sizeof(StaticTicket) + offsetof(TicketRightsHeader, dyn_size), &table_size, sizeof(u32));
		if (R_FAILED(res))
			return std::pair(res, std::nullopt);
		cur_tik_size = sizeof(StaticTicket) + table_size;
		be_int<u64> tikid;
		res = file.Read(cur_tik_start_offset + offsetof(StaticTicket, header.ticket_id), &tikid, sizeof(tikid));
		if (R_FAILED(res))
			return std::pair(res, std::nullopt);
		if (tikid == ticket_id)
		{
			found = true;
			break;
		}
	}
	
	if (!found)
		return std::pair(vfs::NotFound(), std::nullopt);

	ctr::vfs::SubFile sf(file, cur_tik_start_offset, cur_tik_size);
	res = sf.Initialize();
	if (R_FAILED(res))
		return std::pair(res, std::nullopt);

	return std::pair(res, std::move(sf));
}