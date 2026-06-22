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
#ifndef _inc_ctr_vfs_diff_hh
#define _inc_ctr_vfs_diff_hh

#include <3ds.h>

#include <file.hh>
#include <ivfc.hh>
#include <dpfs.hh>
#include <difi.hh>
#include <util.hh>

namespace ctr::vfs
{
	struct __attribute__((packed)) DIFFHeader {
		char magic[4];
		u8 pads[2];
		u16 version;
		u64 secondary_part_desc_offset;
		u64 primary_part_desc_offset;
		u64 part_desc_size;
		u64 part_a_offset;
		u64 part_a_size;
		u32 active_part_desc;
		u8 active_part_desc_hash[0x20];
		u64 unique_identifier;
		u8 reserved[0xA4];
	};
	class DIFFReader : public IFile
	{
	public:
		DIFFReader(IFile& diff_file) : basefile(diff_file), lv3_buf(nullptr), dpfs_desc(nullptr), active_tbl(nullptr), ivfc_desc(nullptr), lvl1(nullptr), lvl2(nullptr) {};
		~DIFFReader();

		Result Initialize() override;
		Result Read(u64 offset, void *buf, u32 size) override;
		u64 GetSize() override;

	private:
		IFile &basefile;

		DIFFHeader hdr;
		u8 *lv3_buf;
		u8 *dpfs_desc;
		u8 *active_tbl;
		u8 *ivfc_desc;
		u32 *lvl1;
		u32 *lvl2;

		DIFIHeader *difi;
		DPFSHeader *dpfs;
		IVFCHeader *ivfc;
	};
}

#endif