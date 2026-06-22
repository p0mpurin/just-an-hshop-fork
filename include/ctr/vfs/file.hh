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
#ifndef _inc_ctr_vfs_file_hh
#define _inc_ctr_vfs_file_hh

#include <3ds.h>

#include <handle.hh>
#include <ctr/vfs/result.hh>

namespace ctr::vfs
{
	class IFile
	{
	public:
		virtual Result Initialize();
		virtual Result Read(u64 offset, void *buf, u32 size);
		virtual u64 GetSize();
		
		template <typename T>
		Result Read(u64 offset, T& out) {
			return Read(offset, &out, sizeof(T));
		}
		
		template <typename T>
		Result Read(u64 offset, void* out) {
			return Read(offset, out, sizeof(T));
		}

	protected:
		IFile() {};
	};

	class FileBase : public IFile {
	public:
		FileBase(ctr::FileHandle basef) : base_file(std::move(basef)) {};

		Result Initialize() override { return FSFILE_GetSize(base_file.Get(), &this->size); }
		Result Read(u64 offset, void *buf, u32 size) override;
		u64 GetSize() override;

		void DetachFile(ctr::FileHandle &target) {
		    target = std::move(base_file);
		}

	protected:
		u64 size;
		ctr::FileHandle base_file;
	};

	class SubFile : public IFile {
	public:
		SubFile(IFile &base) : basef(base) {}
		SubFile(IFile &base, u64 offset, u64 size = U64_MAX) : basef(base), part_offset(offset), part_size(size) {}

		Result Initialize() override {
			if (this->part_size == U64_MAX)
				this->part_size = basef.GetSize() - this->part_offset;

			if (this->part_offset < 0 || this->part_offset > basef.GetSize() ||
				this->part_offset + this->part_size > basef.GetSize())
			{
				return vfs::OutOfBounds();
			}

			return 0;
		}

		Result Initialize(u64 offset, u64 size) {
			this->part_offset = offset;
			this->part_size = size;
			return Initialize();
		}

		Result Read(u64 offset, void *buf, u32 size) override;
		u64 GetSize() override;

	private:
		IFile &basef;
		u64 part_offset;
		u64 part_size;
	};
}

#endif