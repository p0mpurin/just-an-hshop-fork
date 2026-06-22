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

#ifndef inc_util_hh
#define inc_util_hh

#include <functional>
#include <string>
#include <3ds/types.h>

void lower(std::string& s);
void trim(std::string& str, const std::string& whitespace);
void join(std::string& ret, const std::vector<std::string>& tokens, const std::string& sep);

template <typename T, typename = decltype(std::to_string(std::declval<T>()))>
void join_t(std::string& ret, const std::vector<T>& values, const std::string& sep)
{
	if (!values.size()) return;

	ret += std::to_string(values[0]);

	for (size_t i = 1; i < values.size(); i++)
		ret += sep + std::to_string(values[i]);
}

template <typename T>
void join_t(std::string &ret, const std::vector<T>&values, const std::string& sep, std::function<std::string(const T&)> converter)
{
	if (!values.size()) return;

	ret += converter(values[0]);

	for (size_t i = 1; i < values.size(); i++)
		ret += sep + converter(values[i]);
}

std::string u16conv(u16 *str, size_t size);

#endif

