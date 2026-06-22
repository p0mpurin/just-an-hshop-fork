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
#ifndef _inc_ctr_ticket_hh
#define _inc_ctr_ticket_hh

#include <3ds.h>

#include <optional>
#include <utility>

#include <vfs/file.hh>
#include <vfs/util.hh>
#include <irdb.hh>

namespace ctr::ticket
{
	struct __attribute__((packed)) TicketHeader {
		char issuer[64];
		u8 ecc_pubkey[60];
		u8 version;
		u8 ca_crl_version;
		u8 signer_crl_version;
		u8 titlekey[16];
		u8 reserved;
		be_int<u64> ticket_id;
		be_int<u32> console_id;
		be_int<u64> title_id;
		u8 reserved_1[2];
		be_int<u16> ticket_title_version;
		u8 reserved_2[8];
		u8 license_type;
		u8 common_key_y_index;
		u8 reserved_3[42];
		be_int<u32> eshop_account_id;
		u8 reserved_4;
		u8 audit;
	};
	
	static_assert(sizeof(TicketHeader) == 0xE2);
	
	struct __attribute__((packed)) StaticTicket {
		be_int<u32> signature_type;
		u8 signature[256];
		u8 signature_padding[60];
		TicketHeader header;
		u8 reserved_5[66];
		u8 limits[0x40];
	};

	static_assert(sizeof(StaticTicket) == 0x2A4);

	struct TicketRightsHeader
	{
			be_int<u16> version;
			be_int<u16> header_size;
			be_int<u32> dyn_size;
			be_int<u32> descriptors_offset;
			be_int<u16> num_descriptors;
			be_int<u16> descriptor_entry_size;
			be_int<u32> flags;
	};
	
	struct TicketRightDescriptor
	{
			be_int<u32> data_offset;
			be_int<u32> num_entries;
			be_int<u32> entry_size;
			be_int<u32> data_size;
			be_int<u16> type;
			be_int<u16> flags;
	};
	
	template <typename TInt, bool TIsBig>
	struct RawContentRight
	{
			endian_int<TInt, TIsBig> content_index_offset;
			u8 cindex_table[0x80];
	};

	using ContentRight = RawContentRight<u16, false>;
	using ESTicketContentRight = RawContentRight<u32, true>;
	
	class TicketBlob {
		struct BlobHeader {
			u32 num_tickets;
			u32 blob_size;
		};
	public:
		TicketBlob(vfs::IRDBFile& file) : file(file), num_tickets(0) {}

	 	Result Initialize();
		std::pair<Result, std::optional<vfs::SubFile>> OpenTicket(u32 index);
		std::pair<Result, std::optional<vfs::SubFile>> OpenTicket(u64 ticket_id);

		u32 TicketCount() { return num_tickets; }

	private:
		vfs::IRDBFile& file;
		u32 num_tickets;
	};
}

#endif