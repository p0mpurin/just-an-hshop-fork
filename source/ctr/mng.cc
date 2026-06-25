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

#include <ctr/mng.hh>
#include <lzss.hh>

#include <3ds/services/ptmsysm.h>

#define CTR_REGION_ERROR 0xFF
#define CTR_REGION_UNSET 0xFE

template <typename T, size_t N>
inline constexpr size_t array_size(T (&)[N]) { return N; }

Result ctr::mng::list_titles_on(FS_MediaType media, std::vector<ctr::title_id>& ret)
{
	u32 tcount = 0;
	Result res = AM_GetTitleCount(media, &tcount);
	if(R_FAILED(res)) return res;
	
	size_t prevsize = ret.size();
	ret.resize(prevsize + tcount);

	u32 tread = 0;

	res = AM_GetTitleList(&tread, media, tcount, reinterpret_cast<u64 *>(ret.data() + prevsize));
	if(R_FAILED(res)) return res;

	if(tread != tcount)
		return APPERR_TITLE_MISMATCH;

	return res;
}

Result ctr::mng::list_tickets(std::vector<ctr::title_id>& ret)
{
	Result res;
	u32 count, read;

	if(R_FAILED(res = AM_GetTicketCount(&count)))
		return res;
	
	ret.resize(count);

	return AM_GetTicketList(&read, count, 0, reinterpret_cast<u64 *>(ret.data()));
}

Result ctr::mng::get_free_space(ctr::InstallDestination dest, u64 *size)
{
	Result res = 0;

	if (dest == ctr::InstallDestination::TWLNAND) {
		AM_TWLPartitionInfo twl_partition_info { 0, 0, 0, 0 };
		res = AM_GetTWLPartitionInfo(&twl_partition_info);
		if (R_FAILED(res)) return res;
		*size = twl_partition_info.titlesFreeSpace;
		return 0;
	}

	FS_ArchiveResource resource = { 0, 0, 0, 0 };
	res = FSUSER_GetArchiveResource(&resource, dest.to_system_mediatype());
	if (R_FAILED(res)) return res;

	*size = (u64) resource.clusterSize * (u64) resource.freeClusters;
	return res;
}

Result ctr::mng::get_title_entry(ctr::title_id tid, AM_TitleEntry& entry)
{
	return AM_GetTitleInfo(tid.installation_media(), 1, &tid.raw, &entry);
}

// using get info checks is way more efficient than listing everything

bool ctr::mng::title_exists(ctr::title_id tid)
{
	AM_TitleEntry entry;
	return R_SUCCEEDED(AM_GetTitleInfo(tid.installation_media(), 1, &tid.raw, &entry));
}

bool ctr::mng::title_exists_anywhere(ctr::title_id tid)
{
	AM_TitleEntry entry;

	if (R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_SD, 1, &tid.raw, &entry)))
		return true;

	if (R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_GAME_CARD, 1, &tid.raw, &entry)))
		return true;

	if (R_SUCCEEDED(AM_GetTitleInfo(MEDIATYPE_NAND, 1, &tid.raw, &entry)))
		return true;

	return false;
}

bool ctr::mng::ticket_exists(ctr::title_id tid)
{
	AM_TicketInfo entry;
	u32 count = 0;
	return R_SUCCEEDED(AMNET_GetTicketInfos(&count, &entry, 0, 1, tid.raw)) && count == 1;
}

Result ctr::mng::delete_title(ctr::title_id tid, DeleteTitleFlag::Type flags)
{
	Result res = 0;

	if((flags & DeleteTitleFlag::DeleteTicket) && (!(flags & DeleteTitleFlag::CheckExist) || ticket_exists(tid)) && R_FAILED(res = delete_ticket(tid)))
		return res;

	if((!(flags & DeleteTitleFlag::CheckExist) || (ctr::mng::title_exists(tid))) && R_FAILED(res = AM_DeleteTitle(tid.installation_media(), tid.raw)))
		return res;

	return 0;
}

Result ctr::mng::delete_ticket(ctr::title_id tid)
{
	return AM_DeleteTicket(tid.raw);
}

static Result FSUSER_AddSeed(u64 tid, const void *seed)
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = 0x087A0180;
	cmdbuf[1] = (u32) (tid & 0xFFFFFFFF);
	cmdbuf[2] = (u32) (tid >> 32);
	memcpy(&cmdbuf[3], seed, 16);

	Result ret;
	if(R_FAILED(ret = svcSendSyncRequest(*fsGetSessionHandle())))
		return ret;

	ret = cmdbuf[1];
	return ret;
}

static bool seed_is_empty(const u8 (*seed)[16])
{
	static constexpr const u8 empty_seed[16] = { 0 };
	return memcmp(seed, empty_seed, sizeof(empty_seed)) == 0;
}

Result ctr::mng::import_seed(ctr::title_id tid, const u8 (*seed)[16])
{
	if (seed_is_empty(seed))
		return 0;

	return FSUSER_AddSeed(tid.raw, seed);
}

bool ctr::mng::is_n3ds()
{
	static enum {
		No,
		Yes,
		Unchecked,
	} state = Unchecked;
	if(state == Unchecked)
	{
		bool result;
		if(R_FAILED(APT_CheckNew3DS(&result)))
			result = false;
		state = result ? Yes : No;
	}
	return state == Yes;
}

u8 ctr::mng::get_system_region()
{
	static u8 reg = CTR_REGION_UNSET;
	if(reg == CTR_REGION_UNSET)
		if(R_FAILED(CFGU_SecureInfoGetRegion(&reg)))
			reg = CTR_REGION_ERROR;
	return reg;
}

Result ctr::mng::dspfirm::check_existing(const char *path)
{
	Handle dspfirm = 0;
	u64 filesize = 0;
	u32 read = 0;
	char magic[4] = { 0 };

	Result res = FSUSER_OpenFileDirectly(&dspfirm, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_READ, 0);

	if (R_FAILED(res)) return res;

	if (R_FAILED(res = FSFILE_GetSize(dspfirm, &filesize)))
		goto exit;

	if (filesize < 0x104) {
		res = APPERR_DSPFIRM_INVALID;
		goto exit;
	}

	if (R_FAILED(res = FSFILE_Read(dspfirm, &read, 0x100, magic, sizeof(magic))))
		goto exit;

	if (read != sizeof(magic) || memcmp(magic, "DSP1", sizeof(magic)) != 0)
		res = APPERR_DSPFIRM_INVALID;

exit:
	FSFILE_Close(dspfirm);
	return res;
}

extern "C" {
	void *	 memmem (const void *, size_t, const void *, size_t);
};

Result ctr::mng::dspfirm::dump(const char *path)
{
	Result res = 0;
	for (size_t i = 0; i < array_size(ctr::const_ids::home_menu); i++)
	{
		struct {
			ctr::title_id tid;
			FS_MediaType media;
			u32 section;
		} archivepath = {
			ctr::const_ids::home_menu[i],
			MEDIATYPE_NAND,
			0 /* exefs */
		};

		struct {
			u32 section;
			u32 tmd_content_index;
			u32 ncch_section;
			char exefs_filename[8];
		} filepath = {
			0 /* ncch data */,
			0 /* tmd content index */,
			2 /* exefs */,
			".code\0\0"
		};

		FS_Path archive = { PATH_BINARY, sizeof(archivepath), &archivepath },
				file = { PATH_BINARY, sizeof(filepath), &filepath };

		Handle codebin;
		if(R_FAILED(FSUSER_OpenFileDirectly(&codebin, ARCHIVE_SAVEDATA_AND_CONTENT, archive, file, FS_OPEN_READ, 0)))
			continue;

		u64 fsize;
		u8 *dataptr = NULL, *dsp1, *decompressed;
		u32 readb, dsp1size;
		size_t size;
		Handle outfile = 0;
		u32 written;
		if(R_FAILED(FSFILE_GetSize(codebin, &fsize)))
			goto exit;
		if(!(dataptr = new u8[fsize]))
			goto exit;
		if(R_FAILED(FSFILE_Read(codebin, &readb, 0, dataptr, fsize)) || readb != fsize)
			goto exit;

		decompressed = lzss::decompress(dataptr, fsize, &size);
		if(!decompressed) goto exit;
		delete [] dataptr; dataptr = decompressed;

		dsp1 = dataptr;
		do {
			dsp1 = (u8 *)memmem(dsp1, size - ((dsp1 + 1) - dataptr), "DSP1", 4);
			if(!dsp1) goto exit;
			dsp1size = * (u32 *) (dsp1 + 4);
			/* small checks to test if this is really the firmware */
			if(* (u64 *) (&dsp1[0x18]) == 0 && (dsp1[0xE] <= 10 && dsp1[0xE] >= 1) && dsp1[0xD] <= 2 && dsp1size <= size - (dsp1 - dataptr) + 0x100)
				break;
			++dsp1;
		} while(1);

		/* signature is the first 0x100 bytes */
		dsp1 -= 0x100;
		if(R_FAILED((res = FSUSER_OpenFileDirectly(&outfile, ARCHIVE_SDMC, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, path), FS_OPEN_CREATE | FS_OPEN_WRITE, 0))))
			goto exit;
		if(R_FAILED((res = FSFILE_Write(outfile, &written, 0, dsp1, dsp1size, FS_WRITE_FLUSH))))
			goto exit;

		if(written != dsp1size) {
			res = APPERR_DSPFIRM_IO_FAIL;
			goto exit;
		}

		res = 0;
	exit:
		FSFILE_Close(codebin);
		FSFILE_Close(outfile);
		delete [] dataptr;
		if (R_SUCCEEDED(res)) return 0;
		else continue;
	}

	return res;
}

Result ctr::mng::dspfirm::ensure_auto(const char *path)
{
	Result res = ctr::mng::dspfirm::check_existing(path);

	if (R_FAILED(res))
		return ctr::mng::dspfirm::dump(path);

	return 1;
	/* note: 0 = dsp firm dumped, 1 = "valid" dsp firm already exists */
}

void ctr::mng::shutdown_console()
{
	ptmSysmInit();
	PTMSYSM_ShutdownAsync(5000000000LL);
	ptmSysmExit();
}