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
#ifndef _inc_ctr_handle_hh
#define _inc_ctr_handle_hh

#include <3ds.h>

#include <utility>

namespace ctr
{
	class Handle {
	public:
		Handle() : hd(0) {}
		virtual ~Handle() {
			if (hd) svcCloseHandle(hd);
		}

		Handle(Handle &&other) {
			hd = std::exchange(other.hd, 0);
		}

		Handle(const Handle &other) = delete;
		Handle& operator=(Handle &&other) {
			hd = std::exchange(other.hd, 0);
			return *this;
		}
		Handle& operator=(const Handle &other) = delete;

		::Handle& Get() { return hd; }
		
		operator ::Handle() {
			return hd;
		}

	protected:
		::Handle hd;
	};

	class FileHandle : public Handle {
	public:
		FileHandle() : Handle() {};
		~FileHandle() override {
			if (hd) FSFILE_Close(hd);
		}

		FileHandle(FileHandle &&other) {
			hd = std::exchange(other.hd, 0);
		}
		FileHandle(const FileHandle &other) = delete;
		FileHandle& operator=(FileHandle &&other) {
			hd = std::exchange(other.hd, 0);
			return *this;
		}
		FileHandle& operator=(const FileHandle &other) = delete;
	};
}

#endif