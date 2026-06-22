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
 
#include <ctr/titleid.hh>

ctr::title_id::title_id(const std::string& str)
{
	this->raw = strtoull(str.c_str(), nullptr, 16);
}

std::string ctr::title_id::to_string() const
{
	char buf[17];
	snprintf(buf, 17, "%016llX", this->raw);
	return buf;
}
