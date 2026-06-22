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

#ifndef inc_next_hh
#define inc_next_hh

#include "settings.hh"
#include "hsapi.hh"

#define next_cat_exit ((hsapi::hcid) -1)
#define next_sub_back ((hsapi::hcid) -1)
#define next_sub_exit ((hsapi::hcid) -2)

#define next_title_back ((hsapi::hid) -1)
#define next_title_exit ((hsapi::hid) -2)


namespace next
{
	/* used to re-enter the selection screen */
	struct title_reenter_data {
		size_t cursor;
		SortDirection dir;
		SortMethod sortm;
	};

	/* for cat/subcat we only need the cursor to reenter */
	hsapi::hcid sel_cat(size_t *cursor = nullptr);
	hsapi::hcid sel_sub(hsapi::hcid cat, size_t *cursor = nullptr);

	hsapi::hid  sel_title(std::vector<hsapi::PartialTitle>& titles, struct title_reenter_data *rdata = nullptr, bool visited = false);
	void maybe_install_title(std::vector<hsapi::PartialTitle>& titles);

	/* /===/ */
	hsapi::hid sel_icon_title(std::vector<hsapi::PartialTitle>& titles, const hsapi::IndexCategory& cat, const hsapi::IndexSubcategory& subcat);
}

#endif

