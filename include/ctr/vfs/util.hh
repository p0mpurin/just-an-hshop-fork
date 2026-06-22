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
#ifndef _inc_ctr_vfs_util_hh
#define _inc_ctr_vfs_util_hh

#define VTRY(x) res = (x); if (R_FAILED(res)) return res;

#include <3ds.h>

namespace ctr::vfs
{
	u64 floor_64(u64 val, u64 alg);
	u64 floor_64_remain(u64 val, u64 alg);
	u64 align_64(u64 val, u64 alg);
}

template <typename TInt> TInt bswap(TInt);
template <> inline u16 bswap(u16 v) { return __builtin_bswap16(v); }
template <> inline u32 bswap(u32 v) { return __builtin_bswap32(v); }
template <> inline u64 bswap(u64 v) { return __builtin_bswap64(v); }

template <typename TInt, bool is_big>
class __attribute__((packed)) endian_int
{
public:
	endian_int(TInt v = TInt()) : value(v) { }
	
	operator TInt () { return is_big ? bswap(value) : value; }
	endian_int& operator=(TInt new_value) {
		value = is_big ? bswap(new_value) : new_value;
		return *this;
	}
	
	endian_int& operator=(const endian_int<TInt, is_big> &other) = delete;

private:
	TInt value;
};

static_assert(sizeof(endian_int<uint32_t, true>) == sizeof(uint32_t));

template<typename TInt>
using be_int = endian_int<TInt, true>;


#endif