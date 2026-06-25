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

#ifndef inc_update_hh
#define inc_update_hh

#ifdef __3DS__
#include <3ds/types.h>

#include <tuple>
#endif

#ifdef RELEASE
	#define VERSION_SUFFIX ""
#else
	#define VERSION_SUFFIX " (debug)"
#endif

#define VERSION_MAJOR 1
#define VERSION_MINOR 5
#define VERSION_PATCH 37
#define VERSION_DESC "Stable" VERSION_SUFFIX

#define INT_TO_STR(i) INT_TO_STR_(i)
#define INT_TO_STR_(i) #i

#define MK_UA(MA,MI,PA) "hShop (3DS/CTR/KTR; ARMv6) 3hs/" MA "." MI "." PA
#define VERSION INT_TO_STR(VERSION_MAJOR) "." INT_TO_STR(VERSION_MINOR) "." INT_TO_STR(VERSION_PATCH)
#define VVERSION "v" VERSION
#define USER_AGENT MK_UA(INT_TO_STR(VERSION_MAJOR), INT_TO_STR(VERSION_MINOR), INT_TO_STR(VERSION_PATCH))

#ifdef __3DS__

namespace update
{
	enum update_status {
		up_to_date            = 0,
		update_required       = 1,
		outdated_hb_release   = 2,
		updated_successfully  = 3,
		failed_update_check   = 4,
		failed_update_install = 5,
		upstream_update_available = 6,
	};

	struct app_version
	{
		public:
		app_version() { }
		constexpr app_version(u8 maj, u8 min, u8 patch) : maj(maj), min(min), patch(patch) { }

		std::tuple<const u8&, const u8&, const u8&> tie() const { return std::tie(this->maj, this->min, this->patch); }

		bool parse(const char *str, u32 len);

		bool operator==(const app_version& other) const;
		bool operator!=(const app_version& other) const;
		bool operator>(const app_version& other) const;
		bool operator<(const app_version& other) const;
		bool operator<=(const app_version& other) const;
		bool operator>=(const app_version& other) const;

	    u8 maj;
	    u8 min;
	    u8 patch;
	};

	enum update_status update_app(Result &res);
	bool parse_version_string(u32& maj, u32& min, u32& patch, const char *ver_str, u32 len);

	constexpr const app_version CUR_APP_VERSION { VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH };
};

#endif

#endif
