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
#include <vfs/file.hh>

Result ctr::vfs::IFile::Initialize() { return 0xE7E3FFFF; }
Result ctr::vfs::IFile::Read(u64 offset, void *buf, u32 size) { return 0xE7E3FFFF; }
u64 ctr::vfs::IFile::GetSize() { return 0xE7E3FFFF; }

Result ctr::vfs::SubFile::Read(u64 offset, void *buf, u32 size) {
    u64 actual_offset = this->part_offset + offset;
    if (actual_offset < 0 || actual_offset > basef.GetSize() || actual_offset + size > basef.GetSize()) {
        return vfs::OutOfBounds();
    }

    return basef.Read(actual_offset, buf, size);
}

u64 ctr::vfs::SubFile::GetSize() {
    return this->part_size;
}

Result ctr::vfs::FileBase::Read(u64 offset, void *buf, u32 size) {
	u32 read = 0;
    Result res = FSFILE_Read(this->base_file.Get(), &read, offset, buf, size);
    if (R_FAILED(res))
    	return res;
    if (read != size)
    	return vfs::ShortIo();
    return 0;
    	
}

u64 ctr::vfs::FileBase::GetSize() {
    return this->size;
}