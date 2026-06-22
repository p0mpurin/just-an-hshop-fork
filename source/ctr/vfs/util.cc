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
#include <vfs/util.hh>

u64 ctr::vfs::floor_64(u64 val, u64 alg) {
	return val & ~(alg - 1);
}

u64 ctr::vfs::floor_64_remain(u64 val, u64 alg) {
	return val - floor_64(val, alg);
}

u64 ctr::vfs::align_64(u64 val, u64 alg) {
	return (val + alg - 1) & ~(alg - 1);
}