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
#ifndef _inc_ctr_irdb_hh
#define _inc_ctr_irdb_hh

#include <optional>

#include <3ds.h>

#include <ctr/vfs/file.hh>
#include <ctr/vfs/result.hh>

namespace ctr::vfs
{
	struct IRDBPreHeader
	{
		u32 magic;
		u8 pads[2];
		u16 version;
		u64 header_size;
		u64 image_size;
		u64 image_block_size;
	};

	static_assert(sizeof(IRDBPreHeader) == 0x20);

	struct IRDBSectionHeader
	{
		u64 offset;
		u32 count;
	};

	static_assert(offsetof(IRDBSectionHeader, count) == 0x8);
	static_assert(sizeof(IRDBSectionHeader) == 0x10);

	struct IRDBEntryTableHeader
	{
		u32 start_block;
		u32 block_count;
		u32 max_entries;
		u32 _pad;
	};

	static_assert(sizeof(IRDBEntryTableHeader) == 0x10);

	// file entry

	struct __attribute__((packed)) IRDBFileStorageInfo
	{
		u32 start_block_index;
		u64 size;
		u8 small_data[8];
	};

	struct __attribute__((packed)) IRDBFileInfo
	{
		u32 next_sibling_index;
		u8 pad[4];
		IRDBFileStorageInfo StorageInfo;
	};

	struct IRDBDummyFileEntry
	{
		u32 current_total_entry_count;
		u32 max_entry_count;
		u8 __pad[32];
		u32 next_dummy_index;
	};

	struct __attribute__((packed)) IRDBKey
	{
		static const u32 ROOT_DIR = 1;

		u32 parent_dir_idx;
		u64 id;

		static IRDBKey Make(u64 id, u32 parent_dir_idx) {
			return { parent_dir_idx, id };
		}

		bool operator==(const IRDBKey &other) {
			return parent_dir_idx == other.parent_dir_idx && id == other.id;
		}

		bool operator!=(const IRDBKey &other) {
			return !(*this == other);
		}

		IRDBKey &operator=(const IRDBKey &other) {
			parent_dir_idx = other.parent_dir_idx;
			id = other.id;
			return *this;
		}

		u32 Hash();
	};

	struct __attribute__((packed)) IRDBRealFileEntry
	{
		IRDBKey key;
		IRDBFileInfo info;
		u32 hash_bucket_next_index;
	};

	union IRDBFileEntry
	{
		IRDBRealFileEntry entry;
		IRDBDummyFileEntry dummy;
	};

	static_assert(sizeof(IRDBFileEntry) == 0x2C);

	// directory entry

	struct IRDBDirectoryInfo
	{
		u32 next_sibling_dir_index;
		u32 first_subdir_index;
		u32 first_subfile_index;
		u32 a;
	};

	struct __attribute__((packed)) IRDBDummyDirectoryEntry
	{
		u32 current_total_entry_count;
		u32 max_entry_count;
		u8 __pad[16];
		u32 next_dummy_index;
	};

	struct __attribute__((packed)) IRDBRealDirectoryEntry
	{
		u64 id;
		IRDBDirectoryInfo dir_info;
		u32 hash_bucket_next_index;
	};

	union IRDBDirectoryEntry
	{
		IRDBRealDirectoryEntry entry;
		IRDBDummyDirectoryEntry dummy;
	};

	static_assert(sizeof(IRDBDirectoryEntry) == 0x1C);

	// FAT entry

   	struct __attribute__((packed)) IRDBFatHalf {
  		u32 index: 31;
  		u32 flag: 1;
	};

	/*
	* for node head,
	* - U.index --> index of previous node head.
	* - U.flag --> set if this is the first node head.
	* - V.index --> index of next node head.
	* - V.flag --> whether or not this node has extended entries (multiple entries)
	*/
	/*
	* for extended node,
	* - U.index --> index of previous entry in this node.
	* - U.flag --> always set.
	* - V.index --> index of next entry in this node.
	* - V.flag -->  never set.
	*/

	struct __attribute__((packed)) IRDBFatEntry {
		IRDBFatHalf U;
		IRDBFatHalf V;
	};

	static_assert(sizeof(IRDBFatEntry) == 0x8);

	/* forward declaration */
	class IRDBReader;

	class IRDBFile : public IFile {
	public:
		IRDBFile(IRDBReader &reader, const IRDBFileEntry &ent)
			: reader(reader),
			  start_block_index(ent.entry.info.StorageInfo.start_block_index),
			  file_size(ent.entry.info.StorageInfo.size) {}

		Result Initialize() override { return 0; }
		Result Read(u64 offset, void *buf, u32 size) override;
		u64 GetSize() override;

	private:
		u32 FollowFat(Result& out_result, u32 index, bool is_ext, char *&outbuf, u64& total_offset, u64& offset, u32& size);
		Result RecurseFatChain(u32 initial_index, char *outbuf, u64 offset, u32 size);
		IRDBReader &reader;
		u32 start_block_index;
		u32 file_size;
	};

	/* forward declaration */
	class IRDBFileIterator;

	class IRDBReader
	{
		friend class IRDBFile;
	public:
		static const u32 ITERATE_FROM_ROOT = -1;

		IRDBReader(IFile &base_file) : base_fs(base_file), data_region(base_fs), total_data_size(0) {};

		Result Initialize();

		Result ReadFileEntry(IRDBFileEntry *entry, u32 index);
		Result ReadDirectoryEntry(IRDBDirectoryEntry *entry, u32 index);
		Result ReadFileHashtableEntry(IRDBKey *key, u32 *out_entry);
		Result ReadFatEntry(IRDBFatEntry *entry, u32 index);

		Result FindFile(IRDBFileEntry &out_entry, u64 id);

		std::pair<Result, std::optional<IRDBFileIterator>> Iterate(u32 index = ITERATE_FROM_ROOT);

	private:
		IRDBPreHeader pre_header;

		IRDBSectionHeader dir_hashtable_info;
		IRDBSectionHeader file_hashtable_info;
		IRDBSectionHeader fat_info;
		IRDBSectionHeader data_section_info;

		IRDBEntryTableHeader dir_table_info;
		IRDBEntryTableHeader file_table_info;

		u32 data_blocksize;
		IFile& base_fs;
		SubFile data_region;
		u64 total_data_size;
	};

	class IRDBFileIterator {
	public:
		static const Result REACHED_END = VFSResult(RL_INFO, RS_NOP, VFS_END_OF_FILE);

		IRDBFileIterator(IRDBReader &reader) : reader(reader) {}

		Result Begin(u32 index);
		Result Next();

		IRDBFileEntry& operator*() {
			return this->cur_ent;
		}

		IRDBFile Open() {
			return { reader, this->cur_ent };
		}

	private:
		IRDBReader &reader;
		IRDBFileEntry cur_ent;
	};
};

#endif