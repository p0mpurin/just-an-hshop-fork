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
#ifndef _inc_ctr_titleid_hh
#define _inc_ctr_titleid_hh

#include <3ds.h>
#include <string>
#include "panic.hh"

#define CHECK_ALL_REGIONS(x,arr) \
	(((x) == (arr)[0]) || ((x) == (arr)[1]) || ((x) == (arr)[2]) || ((x) == (arr)[3]) || ((x) == (arr)[4]) || ((x) == (arr)[5]))

namespace ctr
{
	namespace const_ids
	{
		constexpr u32 plaza_uniq[6] =
		{
			0x00208, /* JPN */
			0x00218, /* USA */
			0x00228, /* EUR/AUS */
			0x00268, /* CHN */
			0x00278, /* KOR */
			0x00288, /* TWN */
		};

		constexpr u64 home_menu[6] =
		{
			0x0004003000008F02, /* USA */
			0x0004003000008202, /* JPN */
			0x0004003000009802, /* EUR/AUS */
			0x000400300000A102, /* CHN */
			0x000400300000A902, /* KOR */
			0x000400300000B102, /* TWN */
		};
	};
	
	class InstallDestination
	{
	public:
		enum val {
			CTRNAND,
			TWLNAND,
			SDMC,
		};

		constexpr InstallDestination(val v) : value(v) {}

		constexpr operator val() const { return this->value; }
		FS_MediaType to_mediatype() const { return this->value == CTRNAND || this->value == TWLNAND ? MEDIATYPE_NAND : MEDIATYPE_SD; }
		FS_SystemMediaType to_system_mediatype() const {
			switch (this->value) {
				case CTRNAND:
					return SYSTEM_MEDIATYPE_CTR_NAND;
				case TWLNAND:
					return SYSTEM_MEDIATYPE_TWL_NAND;
				case SDMC:
					return SYSTEM_MEDIATYPE_SD;
				default:
					panic("invalid install destination");
			}
		};

		private:
			val value;
	};
	
	struct title_id
	{
	public:
		enum category : u16 {
			normal                    = 0x0,
			dlp_child                 = 0x1,
			demo                      = 0x2,
			content                   = 0x3,
			addon_contents            = 0x4,
			update                    = 0x6,
			noexec                    = 0x8,
			system                    = 0x10,
			require_batch_update      = 0x20,
			not_require_user_approval = 0x40,
			not_require_mount_right   = 0x80,
			can_skip_convert_jumpid   = 0x100,
			twl                       = 0x8000,
			
			dlc_title    = addon_contents | noexec | not_require_mount_right,
			update_title = demo | addon_contents | noexec,
		};


		u64 raw;

		title_id(const std::string& str);
		constexpr title_id(u64 tid) : raw(tid) {}
		constexpr title_id() : raw(0) {}
		std::string to_string() const;

		inline bool operator ==(const title_id& other) const
		{
			return this->raw == other.raw;
		}

		inline bool operator !=(const title_id& other) const
		{
			return this->raw != other.raw;
		}

		inline bool operator >(const title_id& other) const
		{
			return this->raw > other.raw;
		}

		inline bool operator <(const title_id& other) const
		{
			return this->raw < other.raw;
		}

		inline u32 unique_id() const
		{
			return (u32)((this->raw >> 8) & 0xFFFFFF);
		}

		inline category content_category() const
		{
			return (category)((this->raw >> 32) & 0xFFFF);
		}

		inline bool is_base_tid() const
		{
			category cat = this->content_category();
			/* 0x4 = AddOnContents,
			* which updates (0xE) and DLC (0x8C) also include
			* 0x8000 = DSiWare,
			* consider all DSiWare base. For some reason sets 0x4 for all DSiWare */
			return (cat & category::twl) || (cat & category::addon_contents) == 0;
		}

		inline u64 base_tid() const
		{
			/* clear the bits of cat (2nd u16) */
			u64 ret = this->raw & 0xFFFF0000FFFFFFFFLLU;

			if (CHECK_ALL_REGIONS(this->unique_id(), ctr::const_ids::plaza_uniq))
				ret |= 0x1000000000; // add systitle bit

			return ret;
		}
		
		operator u64() const {
			return this->raw;
		}

		// https://www.3dbrew.org/wiki/Titles#Title_IDs
		inline InstallDestination detect_dest() const
		{
			u16 cat = this->content_category();
			return (cat & (category::dlp_child | category::system | category::twl))
				? (cat & category::twl ? ctr::InstallDestination::TWLNAND : ctr::InstallDestination::CTRNAND)
				: ctr::InstallDestination::SDMC;
		}

		inline FS_MediaType installation_media() const { return this->detect_dest().to_mediatype(); }

		inline bool can_have_missing() const
		{
			switch (this->content_category()) {
				case category::normal:
				case category::system:
				case category::twl:
					return true;
				default:
					return false;
			}
		}
	};

	static_assert(sizeof(ctr::title_id) == 8, "title id size is incorrect");
};

#endif