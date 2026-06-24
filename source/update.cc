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

#include "update.hh"

#include <ui/confirm.hh>
#include <ui/base.hh>

#include "installgui.hh"
#include "hsapi.hh"
#include "i18n.hh"
#include "log.hh"

#ifndef NOCTURNE_UPDATE_BASE
	#define NOCTURNE_UPDATE_BASE "http://nocturne.atwebpages.com"
#endif

#define NOCTURNE_UPDATE_CIA_URL NOCTURNE_UPDATE_BASE "/3hs.cia"
#define NOCTURNE_APP_TID 0x0004000003DF1000ULL

update::update_status update::update_app(Result &res)
{
	std::string nver;
	app_version api_version;

	if(R_FAILED(res = hsapi::get_nocturne_latest_version_string(nver)))
		return update_status::failed_update_check;

	if (!api_version.parse(nver.c_str(), nver.size()))
	{
		char badver[9] = { 0 };
		strncpy(badver, nver.c_str(), 8);
		if (!strlen(badver))
			ilog("did not receive any version string from server");
		else
			ilog("received bad version string '%s'... from server", badver);
		res = APPERR_INVALID_VERSION_STRING;
		return update_status::failed_update_check;
	}

	ilog("Fetched latest Nocturne version %s", nver.c_str());

	if(api_version > update::CUR_APP_VERSION)
	{
		ilog("Installing Nocturne update " VERSION " -> %s", nver.c_str());
		if(!ui::Confirm::exec("Nocturne " + nver + " is available.\n\nInstall it now?", "Update available", true))
			return update_status::up_to_date;
		res = install::gui::net_cia(NOCTURNE_UPDATE_CIA_URL,
			ctr::title_id(NOCTURNE_APP_TID), true, true, false, false);
		return R_SUCCEEDED(res)
			? update_status::updated_successfully
			: update_status::failed_update_install;
	}

	if(R_FAILED(res = hsapi::get_latest_version_string(nver)))
	{
		ilog("Official 3hs update check failed (%08lX); Nocturne is up-to-date", res);
		return update_status::up_to_date;
	}

	if (!api_version.parse(nver.c_str(), nver.size()))
	{
		char badver[9] = { 0 };
		strncpy(badver, nver.c_str(), 8);
		if (!strlen(badver))
			ilog("did not receive any official version string from server");
		else
			ilog("received bad official version string '%s'... from server", badver);
		return update_status::up_to_date;
	}

	ilog("Fetched latest official 3hs version %s", nver.c_str());

	if(api_version <= update::CUR_APP_VERSION)
		return update_status::up_to_date;

	ilog("Official 3hs %s is newer than Nocturne's upstream base " VERSION, nver.c_str());
	return update_status::upstream_update_available;
}

bool update::app_version::parse(const char *str, u32 len)
{
	char cbuf[9] = { 0 };
	int ver_nums[3] = { 0, 0, 0 };
	bool at_num = 0;
	int numlen = 0;
	int next_num_index = 0;
	int num_start = 0;

	int max_len = strnlen(str, len);

	if (max_len > 8 || max_len < 5)
		return false;

	len = max_len;

	strncpy(cbuf, str, max_len);

#define IS_DIGIT(x) ((x) >= '0' && (x) <= '9')

	for (int i = 0; i < len; i++)
	{
		char cur = cbuf[i];

		if (!IS_DIGIT(cur) && cur != '.') /* we accept only digits and dots */
			return false;

		if (!at_num && next_num_index == 3)
			return false;

		if (i == 0 && !IS_DIGIT(cur)) /* if the version string doesn't begin with a number, bad */
			return false;

		if (at_num && numlen == 2 && IS_DIGIT(cur)) /* version numbers may not exceed 2 digits */
			return false;

		if (at_num && (cur == '.')) /* number parse end, either if we have a period or a null term */
		{
			ver_nums[next_num_index++] = strtoul(&cbuf[num_start], NULL, 10);
			at_num = false;
			numlen = 0;
			num_start = 0;
			continue;
		}

		if (!at_num && IS_DIGIT(cur)) /* number parse begin */
		{
			if (next_num_index == 3)
				return false; /* the current version format only has 3 numbers max */

			if (cur == '0' && (i + 1 < len && IS_DIGIT(cbuf[i + 1]))) /* allow lone '0', disallow leading '0' */
				return false;

			numlen++;
			at_num = true;
			num_start = i;
			continue;
		}

		if (at_num && IS_DIGIT(cur))
		{
			numlen++;
		}
	}

	if (at_num) {
		cbuf[num_start + numlen] = '\0';
		ver_nums[next_num_index++] = strtoul(&cbuf[num_start], NULL, 10);
	}

	if (cbuf[len - 1] == '.') /* disallow . as the final character */
		return false;

	if (next_num_index != 3) /* if we didn't parse at least 3 numbers, fail */
		return false;

	bool ok = ver_nums[0] <= 63 && ver_nums[1] <= 63 && ver_nums[2] <= 15; /* ensure proper version bounds */

	if (ok)
	{
		this->maj = (u8)ver_nums[0];
		this->min = (u8)ver_nums[1];
		this->patch = (u8)ver_nums[2];
	}

	return ok;
#undef IS_DIGIT
}

bool update::app_version::operator==(const app_version& other) const
{
	return this->tie() == other.tie();
}

bool update::app_version::operator!=(const app_version &other) const
{
	return this->tie() != other.tie();
}

bool update::app_version::operator>(const app_version &other) const
{
	return this->tie() > other.tie();
}

bool update::app_version::operator<(const app_version &other) const
{
	return this->tie() < other.tie();
}

bool update::app_version::operator<=(const app_version &other) const
{
	auto self = this->tie();
	auto othr = other.tie();

	return self < othr || self == othr;
}

bool update::app_version::operator>=(const app_version &other) const
{
	auto self = this->tie();
	auto othr = other.tie();

	return self > othr || self == othr;
}
