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
#ifndef _inc_ctr_smdh_hh
#define _inc_ctr_smdh_hh

#include <3ds.h>

#include <ctr/titleid.hh>

namespace ctr::smdh
{
	enum class RegionLockout : u32
	{
		JPN = 0x01, USA = 0x02, EUR = 0x04,
		AUS = 0x08, CHN = 0x10, KOR = 0x20,
		TWN = 0x40, WORLD = 0x7FFFFFFF,
	};

	enum class Region
	{
		JPN, USA, EUR, // EUR includes AUS
		CHN, KOR, TWN,
		WORLD,
	};

	enum class Ratings : u32
	{
		CERO       = 1 << 0,  // Japan
		ESRB       = 1 << 1,  // USA
		_reserved0 = 1 << 2,
		USK        = 1 << 3,  // Germany
		PEGI_GEN   = 1 << 4,  // Europe
		_reserved1 = 1 << 5,
		PEGI_PRT   = 1 << 6,  // Portugal
		PEGI_BBFC  = 1 << 7,  // England
		COB        = 1 << 8,  // Australia
		GRB        = 1 << 9,  // South Korea
		CGSRR      = 1 << 10, // Taiwan
		_reserved2 = 1 << 11,
		_reserved3 = 1 << 12,
		_reserved5 = 1 << 13,
		_reserved6 = 1 << 14,
		_reserved7 = 1 << 15,
	};

	typedef struct Title
	{
		u16 descShort[0x40]; // wchar
		u16 descLong[0x80]; // wchar
		u16 publisher[0x40]; // wchar
	} Title;

	// https://www.3dbrew.org/wiki/SMDH
	typedef struct SMDH
	{
		// Header
		char magic[0x4]; // constant: 'SMDH'
		u16 version;

		u16 reserved_1;

		// Titles
		smdh::Title titles[0x10];

		// App settings
		u8 ratings[0x10]; // array, index with enum Ratings
		u32 region; // bitfield of enum RegionLockout
		u32 matchMaker;
		u64 matchMakerBit;
		u32 flags;
		u16 eulaVersion;
		u16 reserved_2;
		u32 optimalBnrFrame; // actually a float, but unused so it doesn't matter
		u32 CECID; // streetpass

		u64 reserved_3;

		// GFX
		u8 iconSmall[0x0480]; // 24x24
		u8 iconLarge[0x1200]; // 48x48
	} SMDH;

	ctr::smdh::SMDH *get(ctr::title_id tid);
	ctr::smdh::Title *get_native_title(ctr::smdh::SMDH *smdh);
};

#endif