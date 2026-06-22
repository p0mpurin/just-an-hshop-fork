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

#include <ctr/smdh.hh>

#define AEXEFS_SMDH_PATH             { 0x00000000, 0x00000000, 0x00000002, 0x6E6F6369, 0x00000000 }
#define MAKE_EXEFS_APATH(tid) { (u32) (tid.raw & 0xFFFFFFFF), (u32) ((tid.raw >> 32) & 0xFFFFFFFF), tid.installation_media(), 0x00000000 }
#define SMDH_MAGIC "SMDH"
#define SMDH_MAGIC_LEN 4

#define makebin(data) makebin_(data, sizeof(data))
static inline FS_Path makebin_(const void *path, u32 size)
{
	return { PATH_BINARY, size, path };
}

ctr::smdh::SMDH *ctr::smdh::get(ctr::title_id tid)
{
	static const u32 smdhPath[5] = AEXEFS_SMDH_PATH;
	u32 exefsArchivePath[4] = MAKE_EXEFS_APATH(tid);

	Handle smdhFile;
	if(R_FAILED(FSUSER_OpenFileDirectly(&smdhFile, ARCHIVE_SAVEDATA_AND_CONTENT,
			makebin(exefsArchivePath), makebin(smdhPath), FS_OPEN_READ, 0)))
		return nullptr;

	ctr::smdh::SMDH *ret = new ctr::smdh::SMDH; u32 bread = 0;
	memset(ret, 0x0, sizeof(ctr::smdh::SMDH));

	if(R_FAILED(FSFILE_Read(smdhFile, &bread, 0, ret, sizeof(ctr::smdh::SMDH))))
	{ delete ret; ret = nullptr; goto finish; }

	// Invalid smdh
	if(memcmp(ret->magic, SMDH_MAGIC, SMDH_MAGIC_LEN) != 0)
	{ delete ret; ret = nullptr; goto finish; }

finish:
	FSFILE_Close(smdhFile);
	return ret;
}

ctr::smdh::Title *ctr::smdh::get_native_title(ctr::smdh::SMDH *smdh)
{
	ctr::smdh::Title *title = nullptr;
	u8 syslang;

	if(R_SUCCEEDED(CFGU_GetSystemLanguage(&syslang)))
	{
		switch(syslang)
		{
		case CFG_LANGUAGE_JP: title = &smdh->titles[0]; break;
		case CFG_LANGUAGE_EN: title = &smdh->titles[1]; break;
		case CFG_LANGUAGE_FR: title = &smdh->titles[2]; break;
		case CFG_LANGUAGE_DE: title = &smdh->titles[3]; break;
		case CFG_LANGUAGE_IT: title = &smdh->titles[4]; break;
		case CFG_LANGUAGE_ES: title = &smdh->titles[5]; break;
		case CFG_LANGUAGE_ZH: title = &smdh->titles[6]; break;
		case CFG_LANGUAGE_KO: title = &smdh->titles[7]; break;
		case CFG_LANGUAGE_NL: title = &smdh->titles[8]; break;
		case CFG_LANGUAGE_PT: title = &smdh->titles[9]; break;
		case CFG_LANGUAGE_RU: title = &smdh->titles[10]; break;
		case CFG_LANGUAGE_TW: title = &smdh->titles[11]; break;
		}
	}

	if(title != nullptr && title->descShort[0] != 0)
		return title;

	// EN, JP, FR, DE, IT, ES, ZH, KO, NL, PT, RU, TW
	static const u8 lookuporder[] = { 1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	for(u8 i = 0; i < sizeof(lookuporder); ++i)
	{
		if(smdh->titles[i].descShort[0] != 0)
			return &smdh->titles[i];
	}

	return nullptr;
}
