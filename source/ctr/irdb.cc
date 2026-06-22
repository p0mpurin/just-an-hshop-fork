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
#include <irdb.hh>
#include <vfs/util.hh>

#include <climits>

#include <optional>
#include <algorithm>

#define T(x) res = (x); if (R_FAILED(res)) return res;

static inline uint32_t rotr32 (uint32_t n, unsigned int c)
{
	const unsigned int mask = (CHAR_BIT * sizeof(n) - 1);
	c &= mask;
	return (n >> c) | (n << ( (-c)&mask ));
}

u32 ctr::vfs::IRDBKey::Hash() {
	u32 hash = 0x12345678;

	for (int i = 0; i < 3; i++) {
		hash = rotr32(hash, 1) ^ reinterpret_cast<u32 *>(this)[i];
	}

	return hash;
}

Result ctr::vfs::IRDBReader::Initialize()
{
	Result res = 0;

	VTRY(base_fs.Initialize());

	if (base_fs.GetSize() < sizeof(ctr::vfs::IRDBPreHeader))
		return vfs::DataTooShort();

	u64 r_offset = 0;

	VTRY(base_fs.Read(r_offset, &this->pre_header, sizeof(ctr::vfs::IRDBPreHeader)));

	if (pre_header.magic != 0x49524442) // IRDB
		return vfs::MagicMismatch();

	if (pre_header.version != 3)
		return vfs::UnsupportedVersion();

	if (pre_header.image_size * pre_header.image_block_size > base_fs.GetSize())
		return vfs::DataTooShort();

	VTRY(base_fs.Read(0x24, &this->data_blocksize, sizeof(this->data_blocksize)));

	VTRY(base_fs.Read(0x28, &this->dir_hashtable_info, sizeof(IRDBSectionHeader)));
	VTRY(base_fs.Read(0x38, &this->file_hashtable_info, sizeof(IRDBSectionHeader)));
	VTRY(base_fs.Read(0x48, &this->fat_info, sizeof(IRDBSectionHeader)));
	VTRY(base_fs.Read(0x58, &this->data_section_info, sizeof(IRDBSectionHeader)));
	VTRY(base_fs.Read(0x68, &this->dir_table_info, sizeof(IRDBEntryTableHeader)));
	VTRY(base_fs.Read(0x78, &this->file_table_info, sizeof(IRDBEntryTableHeader)));

	this->total_data_size = this->data_blocksize * data_section_info.count;

	VTRY(this->data_region.Initialize(this->data_section_info.offset, this->total_data_size));

	return res;
}

Result ctr::vfs::IRDBReader::ReadFileHashtableEntry(IRDBKey *key, u32 *out_entry)
{
	u32 hashtable_index = key->Hash() % file_hashtable_info.count;
	return base_fs.Read(file_hashtable_info.offset + 4 * hashtable_index, out_entry, 4);
}

Result ctr::vfs::IRDBReader::ReadFatEntry(IRDBFatEntry *entry, u32 index)
{
	return base_fs.Read(fat_info.offset + sizeof(IRDBFatEntry) * index, entry, sizeof(IRDBFatEntry));
}

Result ctr::vfs::IRDBReader::ReadDirectoryEntry(IRDBDirectoryEntry *entry, u32 index)
{
	u64 offset = this->data_section_info.offset + (dir_table_info.start_block * this->data_blocksize) + index * sizeof(IRDBDirectoryEntry);
	return base_fs.Read(offset, entry, sizeof(IRDBDirectoryEntry));
}

Result ctr::vfs::IRDBReader::ReadFileEntry(IRDBFileEntry *entry, u32 index)
{
	u64 offset = this->data_section_info.offset + (file_table_info.start_block * this->data_blocksize) + index * sizeof(IRDBFileEntry);
	return base_fs.Read(offset, entry, sizeof(IRDBFileEntry));
}

Result ctr::vfs::IRDBReader::FindFile(IRDBFileEntry &out_entry, u64 id)
{
	Result res = 0;
	u32 bucket_first_idx = 0;

	IRDBKey key = IRDBKey::Make(id, IRDBKey::ROOT_DIR);
	IRDBRealFileEntry &ent = out_entry.entry;

	VTRY(ReadFileHashtableEntry(&key, &bucket_first_idx));

	VTRY(ReadFileEntry(&out_entry, bucket_first_idx));

	if (ent.key == key)
		return 0;

	while (ent.hash_bucket_next_index) {
		VTRY(ReadFileEntry(&out_entry, ent.hash_bucket_next_index));

		if (ent.key == key)
			return 0;
	}

	return vfs::NotFound();
}

std::pair<Result, std::optional<ctr::vfs::IRDBFileIterator>> ctr::vfs::IRDBReader::Iterate(u32 index)
{
	Result res = 0;

	u32 actual_index = index;

	IRDBFileIterator it(*this);

	if (index == IRDBReader::ITERATE_FROM_ROOT)
	{
		IRDBDirectoryEntry root_dir;
		res = ReadDirectoryEntry(&root_dir, IRDBKey::ROOT_DIR);
		if (R_FAILED(res))
			return std::pair(res, std::nullopt);

		actual_index = root_dir.entry.dir_info.first_subfile_index;
	}

	res = it.Begin(actual_index);
	if (R_FAILED(res))
		return std::pair(res, std::nullopt);

	return std::pair(0, it);
}

Result ctr::vfs::IRDBFileIterator::Begin(u32 index) {
	Result res = 0;

	res = reader.ReadFileEntry(&cur_ent, index);

	return R_FAILED(res) && R_MODULE(res) != RM_APPLICATION ?
		res :
		REACHED_END;
}

Result ctr::vfs::IRDBFileIterator::Next() {
	if (!cur_ent.entry.info.next_sibling_index)
		return REACHED_END;

	return reader.ReadFileEntry(&cur_ent, cur_ent.entry.info.next_sibling_index);
}

u32 ctr::vfs::IRDBFile::FollowFat(Result &out_res, u32 index, bool is_ext, char *&outbuf, u64& total_offset, u64& offset, u32& size) {
	IRDBFatEntry ent;

	if (R_FAILED(out_res = reader.ReadFatEntry(&ent, index)))
		return UINT32_MAX;

	if (index) {
		u32 readsize = 0;
		if (total_offset + reader.data_blocksize >= offset) {
			u32 block_offset = (offset - total_offset); /* should be 0 when offset is 0 or aligned to data blocksize */
			u32 block_remain = reader.data_blocksize - block_offset;
			u64 dr_offset = (index - 1) * (reader.data_blocksize) + block_offset;
			readsize = std::min(size, block_remain);
			if (R_FAILED(out_res = reader.data_region.Read(dr_offset, outbuf, readsize)))
				return UINT32_MAX;
			size -= readsize;
			offset += readsize;
			outbuf += readsize;
		}

		if (!size) return 0;

		total_offset += reader.data_blocksize;
	}

	if (is_ext) return 0;
	/* process extended nodes first. */
	if (ent.V.flag) {
		/* get the end index of this node */
		IRDBFatEntry after_ent;
		// this should be replaced with some other method to avoid stack overflow
		// fat entries are very small though (and tickets are as well, meaning less fat entries)
		// so this should be fine?
		if (R_FAILED(out_res = reader.ReadFatEntry(&after_ent, index + 1)))
			return UINT32_MAX;

		u32 end_index = after_ent.V.index;
		for (u32 i = index + 1; i < end_index + 1; i++) {
			if (R_FAILED(out_res = FollowFat(out_res, i, true, outbuf, total_offset, offset, size)))
				return UINT32_MAX;
		}
	}

	if (ent.V.index) {
		/* go to next node, if we have one. */
		return ent.V.index;
	}

	return 0; // done
}

Result ctr::vfs::IRDBFile::RecurseFatChain(u32 initial_index, char *outbuf, u64 offset, u32 size) {
	u32 idx = initial_index;
	Result res = 0;

	char *_outbuf = outbuf;
	u64 _full_offset = 0;
	u64 _offset = offset;
	u32 _size = size;

	do {
		idx = FollowFat(res, idx, false, _outbuf, _full_offset, _offset, _size);
	}
	while (idx != 0 && idx != UINT32_MAX);

	return res;
}

Result ctr::vfs::IRDBFile::Read(u64 offset, void *buf, u32 size)
{
	if (offset < 0 || offset > this->file_size || offset + size > this->file_size)
		return vfs::OutOfBounds();

	return RecurseFatChain(this->start_block_index + 1, reinterpret_cast<char *>(buf), offset, size);
}

u64 ctr::vfs::IRDBFile::GetSize() {
	return this->file_size;
}