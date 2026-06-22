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
#ifndef _inc_ctr_vfs_ivfc_hh
#define _inc_ctr_vfs_ivfc_hh

#include <3ds.h>

#include <log2_size.hh>

namespace ctr::vfs
{
	struct IVFCLevel {
		u64 offset;
		u64 size;
		log2_size<u32> blocksize;
		u32 pad;
	};

	struct __attribute__((packed)) IVFCTree {
		u32 unk;
		IVFCLevel level1;
		IVFCLevel level2;
		IVFCLevel level3;
		IVFCLevel level4;
	};

	struct IVFCHeader
	{
		u32 magic;
		u16 pad;
		u16 version;
		u32 master_hash_size;
		IVFCTree tree;
		u32 tree_size;
		u32 tree_start_offset;
	};
}

#endif