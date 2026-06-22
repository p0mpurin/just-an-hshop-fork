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
#ifndef _inc_ctr_vfs_difi_hh
#define _inc_ctr_vfs_difi_hh

#include <3ds.h>

namespace ctr::vfs
{
	struct __attribute__((packed)) DIFIHeader {
		char magic[4];
		u32 magic2;
		// all relative to part desc off
		u64 ivfc_desc_off;
		u64 ivfc_desc_size;
		u64 dpfs_desc_off;
		u64 dpfs_desc_size;
		u64 part_hash_offs;
		u64 part_hash_size;
		u8 enable_ivfc_lvl4;
		u8 dpfs_lvl1_select;
		u16 pad;
		u64 external_ivfc_lvl4_off; // relative to part off
	};
}

#endif