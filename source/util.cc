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

#include <widgets/konami.hh>

#include <ui/base.hh>

#include "panic.hh"
#include "util.hh"

void lower(std::string& s)
{
	for(size_t i = 0; i < s.size(); ++i)
		s[i] = tolower(s[i]);
}

// https://stackoverflow.com/questions/1798112/removing-leading-and-trailing-spaces-from-a-string#1798170
void trim(std::string& str, const std::string& whitespace)
{
	const size_t str_begin = str.find_first_not_of(whitespace);
	if(str_begin == std::string::npos) { str = ""; return; }

	const size_t str_end = str.find_last_not_of(whitespace);
	const size_t str_range = str_end - str_begin + 1;

	str = str.substr(str_begin, str_range);
}

void join(std::string& ret, const std::vector<std::string>& tokens, const std::string& sep)
{
	if(tokens.size() == 0) { ret = ""; return; }
	ret = tokens[0];
	for(size_t i = 1; i < tokens.size(); ++i)
		ret += sep + tokens[i];
}

std::string u16conv(u16 *str, size_t size)
{
	u16 *strexpand = (u16 *) malloc(sizeof(u16) * (size + 1));
	memcpy(strexpand, str, size * sizeof(u16));
	strexpand[size] = 0; /* all this for a NULL term */

	u8 *buf = (u8 *) malloc(size * 4);
	ssize_t units = utf16_to_utf8(buf, str, size * 4);
	if(units == -1 || units > (ssize_t) size * 4)
		panic("Failed to decode utf16 sequence, units=" + std::to_string(units));

	free((void *) strexpand);
	std::string ret((char *) buf, units);
	free((void *) buf);
	return ret;
}