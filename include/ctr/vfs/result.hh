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
#ifndef _inc_ctr_vfs_result_hh
#define _inc_ctr_vfs_result_hh

#include <3ds.h>

namespace ctr::vfs {

	const int RM_CTR_VFS = RM_APPLICATION - 1;

	enum Error {
		VFS_MAGIC_MISMATCH      = 0,
		VFS_DATA_TOO_SHORT      = 1,
		VFS_OUT_OF_BOUNDS       = 2,
		VFS_END_OF_FILE         = 3,
		VFS_UNSUPPORTED_VERSION = 4,
		VFS_NOT_FOUND           = 5,
		VFS_SHORT_IO            = 6,
	};

	constexpr inline Result VFSResult(int level, int summary, int description) {
		return MAKERESULT(level, summary, RM_CTR_VFS, description);
	}

	constexpr inline Result OutOfBounds() {
		return VFSResult(RL_USAGE, RS_INVALIDARG, VFS_OUT_OF_BOUNDS);
	}

	constexpr inline Result DataTooShort() {
		return VFSResult(RL_PERMANENT, RS_INVALIDARG, VFS_DATA_TOO_SHORT);
	}

	constexpr inline Result MagicMismatch() {
		return VFSResult(RL_PERMANENT, RS_INVALIDARG, VFS_MAGIC_MISMATCH);
	}

	constexpr inline Result UnsupportedVersion() {
		return VFSResult(RL_PERMANENT, RS_INVALIDARG, VFS_UNSUPPORTED_VERSION);
	}

	constexpr inline Result NotFound() {
		return VFSResult(RL_STATUS, RS_CANCELED, VFS_NOT_FOUND);
	}

	constexpr inline Result OutOfMemory() {
		return VFSResult(RL_FATAL, RS_OUTOFRESOURCE, RD_OUT_OF_MEMORY);
	}

	constexpr inline Result ShortIo() {
		return VFSResult(RL_STATUS, RS_CANCELED, VFS_SHORT_IO);
	}
}

#endif