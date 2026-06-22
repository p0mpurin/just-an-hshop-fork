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
#include <vfs/diff.hh>
#include <ctr/vfs/util.hh>
#include <cstdio>

#include <stdlib.h>

static inline u8 bit(u32 *bitarr, u32 bi) {
	return (bitarr[bi / 32] >> (31 - (bi % 32))) & 1;
}

static inline u8 bit_select(u32 *bitarr, u32 bi, u64 lvlsize, u8 select) {
	return bit(&bitarr[select * lvlsize / 4], bi);
}

Result ctr::vfs::DIFFReader::Initialize() {

	Result res = -1;

	VTRY(basefile.Read(0x100, &this->hdr, sizeof(ctr::vfs::DIFFHeader)));

	/* partition table = partition descriptor */

	this->active_tbl = (u8 *)malloc(this->hdr.part_desc_size);
	if (!this->active_tbl)
		return vfs::OutOfMemory();

	u64 active_table_off = this->hdr.active_part_desc ?
								this->hdr.secondary_part_desc_offset :
								this->hdr.primary_part_desc_offset;

	VTRY(basefile.Read(active_table_off, this->active_tbl, this->hdr.part_desc_size))

	/* this is for partitionA */

	this->difi = reinterpret_cast<ctr::vfs::DIFIHeader *>(this->active_tbl); /* need for dpfs and ivfc */

	u64 dpfs_desc_off = active_table_off + this->difi->dpfs_desc_off;
	this->dpfs_desc = (u8 *)malloc(this->difi->dpfs_desc_size);
	if (!this->dpfs_desc)
		return vfs::OutOfMemory();

	VTRY(basefile.Read(dpfs_desc_off, this->dpfs_desc, this->difi->dpfs_desc_size));

	u64 ivfc_desc_off = active_table_off + this->difi->ivfc_desc_off;
	this->ivfc_desc = (u8 *)malloc(this->difi->ivfc_desc_size);
	if (!this->ivfc_desc)
		return vfs::OutOfMemory();

	VTRY(basefile.Read(ivfc_desc_off, this->ivfc_desc, this->difi->ivfc_desc_size));

	this->dpfs = reinterpret_cast<ctr::vfs::DPFSHeader *>(this->dpfs_desc);
	this->ivfc = reinterpret_cast<ctr::vfs::IVFCHeader *>(this->ivfc_desc);

	/* we can read lvl1 and lvl2 entirely; they shouldn't be that large */
	u64 actual_lv1_off = (this->hdr.part_a_offset + this->dpfs->lvl1.offset) + (this->difi->dpfs_lvl1_select * this->dpfs->lvl1.size);
	u64 actual_lv2_off = this->hdr.part_a_offset + this->dpfs->lvl2.offset;

	this->lvl1 = (u32 *)malloc(this->dpfs->lvl1.size * 2);
	if (!this->lvl1)
		return vfs::OutOfMemory();

	VTRY(basefile.Read(actual_lv1_off, this->lvl1, this->dpfs->lvl1.size * 2));

	this->lvl2 = (u32 *)malloc(this->dpfs->lvl2.size * 2);
	if (!this->lvl2)
		return vfs::OutOfMemory();

	VTRY(basefile.Read(actual_lv2_off, this->lvl2, this->dpfs->lvl2.size * 2));

	this->lv3_buf = (u8 *)malloc(this->dpfs->lvl3.blocksize.Pow2());
	if (!this->lv3_buf)
		return vfs::OutOfMemory();

	return 0;
}

ctr::vfs::DIFFReader::~DIFFReader() {
	if (this->lv3_buf) { free(this->lv3_buf); this->lv3_buf = NULL; }
	if (this->dpfs_desc) { free(this->dpfs_desc); this->dpfs_desc = NULL; }
	if (this->active_tbl) { free(this->active_tbl); this->active_tbl = NULL; }
	if (this->lvl1) { free(this->lvl1); this->lvl1 = NULL; }
	if (this->lvl2) { free(this->lvl2); this->lvl2 = NULL; }
}

Result ctr::vfs::DIFFReader::Read(u64 offset, void *buf, u32 size) {
	Result res = 0;
	u32 read = 0;

	u64 total_pa_size = this->dpfs->lvl3.size;

	if (offset < 0 || offset > GetSize() || offset + size > GetSize())
		return vfs::OutOfBounds();

	u8 *_buf = reinterpret_cast<u8 *>(buf);

	u64 start_offset = floor_64(this->ivfc->tree.level4.offset + offset, this->dpfs->lvl3.blocksize.Pow2());
	u64 end_offset = std::min(align_64(start_offset + size, this->dpfs->lvl3.blocksize.Pow2()), total_pa_size);

	u64 cur_block_offset = start_offset;

	while (size && cur_block_offset < end_offset) {
		u64 lvl2bitidx = cur_block_offset / this->dpfs->lvl3.blocksize.Pow2();
		u64 lvl2idx = lvl2bitidx / 8;
		u64 lvl1bitidx = lvl2idx / this->dpfs->lvl2.blocksize.Pow2();

		u64 actual_lv3_off = this->hdr.part_a_offset + this->dpfs->lvl3.offset;

		u8 lv2_select = bit(this->lvl1, lvl1bitidx);
		u8 lv3_select = bit_select(this->lvl2, lvl2bitidx, this->dpfs->lvl2.size, lv2_select);

		u64 final_lv3_off = (actual_lv3_off + (this->dpfs->lvl3.size * lv3_select)) + cur_block_offset;

		if (cur_block_offset == start_offset && floor_64_remain(offset, this->dpfs->lvl3.blocksize.Pow2()))
			final_lv3_off += floor_64_remain(offset, this->dpfs->lvl3.blocksize.Pow2());

		u32 readsize = std::min(this->dpfs->lvl3.blocksize.Pow2(), size);
		VTRY(basefile.Read(final_lv3_off, &_buf[read], readsize));

		size -= read;

	    cur_block_offset += this->dpfs->lvl3.blocksize.Pow2();
	}

	return res;
}

u64 ctr::vfs::DIFFReader::GetSize()
{
	return this->dpfs->lvl3.size - this->ivfc->tree.level4.offset;
}
