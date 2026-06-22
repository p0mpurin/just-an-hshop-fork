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

#include "log.hh"
#include "mng.hh"
#include "titleid.hh"
#include <dlc_fixer.hh>
#include <panic.hh>
#include <ui/base.hh>
#include <ui/loading.hh>
#include <ui/progress_bar.hh>
#include <ui/confirm.hh>

#include <3ds.h>
#include <algorithm>
#include <malloc.h>
#include <ctr/vfs/file.hh>
#include <ctr/vfs/util.hh>
#include <ctr/ticket.hh>
#include <ctr/ticketdb.hh>

#define MAX_TICKET_SIZE 0x23CC

void show_error(Result err, const char *msg) {
	error_container e = get_error(err);
	report_error(e, msg);
	handle_error(e);
}
using StaticTik = ctr::ticket::StaticTicket;
using TikHdr = ctr::ticket::TicketHeader;
using RightHdr = ctr::ticket::TicketRightsHeader;
using RightDesc = ctr::ticket::TicketRightDescriptor;
using EsContentRight = ctr::ticket::ESTicketContentRight;
using ContentRight = ctr::ticket::ContentRight;
using IRDBFileEnt = ctr::vfs::IRDBFileEntry;
using Ticket = ctr::ticket::TicketBlob;

static Result FixTicket(u8 *outbuf, u32 &newsize, const TikHdr &in_min, u32 num_contents) {
	StaticTik *out_static_tik = reinterpret_cast<StaticTik *>(outbuf);

	// rebuild fake signature
	out_static_tik->signature_type = 0x00010004;
	memset(&out_static_tik->signature, 0, sizeof(out_static_tik->signature) + sizeof(out_static_tik->signature_padding));
	memcpy(&out_static_tik->header, &in_min, sizeof(TikHdr));
	memset(&out_static_tik->reserved_5, 0, sizeof(out_static_tik->reserved_5) + sizeof(out_static_tik->limits));

	/* console ID = 0, to prevent corruption of the titlekey */
	out_static_tik->header.console_id = 0;

	u32 n_records = ((num_contents + 1024 - 1) & ~(1024 - 1)) / 1024;

	RightHdr *main_hdr = reinterpret_cast<RightHdr *>(&outbuf[sizeof(StaticTik)]);
	RightDesc *content_hdr = reinterpret_cast<RightDesc *>(&outbuf[sizeof(StaticTik) + sizeof(RightHdr)]);

	main_hdr->version = 1;
	main_hdr->header_size = sizeof(RightHdr);
	main_hdr->dyn_size = sizeof(RightHdr) + sizeof(RightDesc) + sizeof(EsContentRight) * n_records;
	main_hdr->descriptors_offset = sizeof(RightHdr);
	main_hdr->num_descriptors = 1;
	main_hdr->descriptor_entry_size = sizeof(RightDesc);
	main_hdr->flags = 0;

	content_hdr->data_offset = main_hdr->descriptors_offset + sizeof(RightDesc);
	content_hdr->num_entries = n_records;
	content_hdr->entry_size = sizeof(EsContentRight);
	content_hdr->data_size = n_records * sizeof(EsContentRight);
	content_hdr->type = 3;
	content_hdr->flags = 0;

	EsContentRight *cur_record = reinterpret_cast<EsContentRight *>(&outbuf[sizeof(StaticTik) + sizeof(RightHdr) + sizeof(RightDesc)]);

	for (u32 i = 0; i < n_records; i++) {
		cur_record[i].content_index_offset = i * 1024;
		memset(cur_record[i].cindex_table, 0xFF, sizeof(cur_record[i].cindex_table));
	}

	newsize = sizeof(StaticTik) + main_hdr->dyn_size;
	return 0;
}

#define TCHECKR(_msg, _res) if (R_FAILED(_res)) { show_error(_res, _msg); return; }
#define TCHECK(_msg) TCHECKR(_msg, res)
#define TTRY(_exp,_msg) res = (_exp); TCHECK(_msg)
#define TTRYV(_exp,_msg) \
	({ auto [rt, value] = (_exp); \
	TCHECKR(_msg,rt); \
	value; })

void show_dlc_fixer()
{
	ui::RenderQueue rq;
	ui::ProgressBar *prog = nullptr;
	ui::Text *progdesc = nullptr;
	Result res = 0;

	ui::builder<ui::ProgressBar>(ui::progloc())
		.y(ui::layout::center_y)
		.add_to(&prog, rq);

	ui::builder<ui::Text>(ui::progloc())
		.x(ui::layout::center_x)
		.wrap()
		.add_to(&progdesc, rq);

	float center_above_progbar = (rq.back()->get_y() - progdesc->height()) / 2.0f;
	progdesc->set_y(center_above_progbar + 15.0f);

	u32 total_tiks_count = 0;
	std::vector<ctr::title_id> sd_titles;
	std::vector<AM_TicketInfo> ticket_infos;
	std::vector<AM_TicketInfo> tiks_to_fix;

	ui::loading([&res, &sd_titles]() -> void {
		res = ctr::mng::list_titles_on(MEDIATYPE_SD, sd_titles);
	}, str::listing_sd_titles);

	TCHECK("user was in listing titles step of DLC fixer");

	sd_titles.erase(std::remove_if(sd_titles.begin(), sd_titles.end(), [](const ctr::title_id &tid) -> bool {
		return tid.content_category() != ctr::title_id::dlc_title;
	}), sd_titles.end());

	progdesc->set_text(str::listing_tickets);
	prog->setup(0, sd_titles.size());
	prog->activate();

	for (size_t i = 0; i < sd_titles.size(); i++) {
		const ctr::title_id& dlc_tid = sd_titles[i];
		u32 num = 0;
		ilog("AMNET_GetTitleNumTickets(%016llX)", dlc_tid);
		res = AMNET_GetTitleNumTickets(&num, dlc_tid);
		ilog("AMNET_GetTitleNumTickets(%016llX) = %08lX", res);
		if (R_FAILED(res)) {
			if (res == 0xD8A083FA) continue;
			else TCHECK("couldn't get number of tickets");
		}

		size_t osize = ticket_infos.size();
		ticket_infos.resize(osize + num);

		ilog("AMAPP_ListDLCOrLicenseTicketInfos(%u, %016llX)", num, dlc_tid);
		TTRY(AMAPP_ListDLCOrLicenseTicketInfos(&num, ticket_infos.data() + osize, 0, num, dlc_tid), "couldn't list tickets");
		ilog("AMAPP_ListDLCOrLicenseTicketInfos(%u, %016llX) = %08lX", res);
		prog->update(i + 1, sd_titles.size());
		rq.render_frame();
	}

	tiks_to_fix.reserve(ticket_infos.size());

	progdesc->set_text(str::checking_broken_dlctiks);
	prog->setup(0, ticket_infos.size());

	for (size_t i = 0; i < ticket_infos.size(); i++) {
		const AM_TicketInfo& tikinfo = ticket_infos[i];
		u32 offset = 0;
		while (true) {
			ContentRight cur_right;
			u32 total_count = 0;
			u32 num_read_cur = 0;
			TTRY(AMAPP_GetDLCOrLicenseItemRights(
				&total_count,
				&num_read_cur,
				&cur_right,
				sizeof(ContentRight),
				3,
				offset,
				tikinfo.titleId,
				tikinfo.ticketId
			), "couldn't check rights");

			for (u32 k = 0; k < 0x80; k++) {
				if (cur_right.cindex_table[k] != 0xFF) {
					tiks_to_fix.push_back(tikinfo);
					break;
				}
			}

			if (num_read_cur + offset == total_count)
				break;

			offset++;
		}
		prog->update(i + 1, ticket_infos.size());
		rq.render_frame();
	}

	prog->set_hidden(true);

	if (!tiks_to_fix.size()) {
		progdesc->set_text(str::no_dlc_to_fix);
		rq.render_finite_button(KEY_A);
		return;
	}

	if (!ui::Confirm::exec(PSTRING(fix_n_dlc_tickets, tiks_to_fix.size())))
		return;

	progdesc->set_text(str::opening_ticket_db);
	rq.render_frame();

	ctr::FileHandle ticketdb_file;

	TTRY(FSUSER_OpenFileDirectly(
			&ticketdb_file.Get(),
			ARCHIVE_NAND_CTR_FS,
			fsMakePath(PATH_EMPTY, ""),
			fsMakePath(PATH_ASCII, "/dbs/ticket.db"), FS_OPEN_READ, 0
		), "user was in the ticket.db reading stage of the DLC fixer");

	ctr::TicketDatabase ticketdb(std::move(ticketdb_file));

	TTRY(ticketdb.Initialize(), "user was in ticket.db init stage of DLC fixer");

	std::vector<TikHdr> to_fix_hdrs;
	to_fix_hdrs.resize(tiks_to_fix.size());

	progdesc->set_text(str::extracting_dlc_tickets);
	prog->set_hidden(false);
	prog->setup(0, tiks_to_fix.size());

	for (size_t i = 0; i < tiks_to_fix.size(); i++) {
		const AM_TicketInfo& tik_to_fix = tiks_to_fix[i];
		IRDBFileEnt file_entry;

		TTRY(ticketdb.FindTicket(file_entry, tik_to_fix.titleId), "couldn't find ticket blob");

		ctr::vfs::IRDBFile tik_blob_raw = ticketdb.OpenTicket(file_entry);
		Ticket tikblob(tik_blob_raw);

		TTRY(tikblob.Initialize(), "couldn't initialize ticket blob");

		auto tik = TTRYV(tikblob.OpenTicket(tik_to_fix.ticketId), "couldn't open ticket");

		TTRY(tik->Read(offsetof(StaticTik, header), to_fix_hdrs.data() + i, sizeof(TikHdr)), "couldn't read ticket header");
		prog->update(i + 1, tiks_to_fix.size());
		rq.render_frame();
	}

	progdesc->set_text(str::installing_fixed_dlc_tickets);
	prog->setup(0, tiks_to_fix.size());

	u8 fulltik[MAX_TICKET_SIZE];
	for (size_t i = 0; i < tiks_to_fix.size(); i++) {
		const AM_TicketInfo& tik_to_fix = tiks_to_fix[i];
		const TikHdr& tik_hdr = to_fix_hdrs[i];

		u32 num_contents = 0;
		u32 new_tik_size = 0;
		u32 written = 0;
		ctr::Handle tik_import;

		TTRY(AMAPP_GetDLCContentInfoCount(&num_contents, MEDIATYPE_SD, tik_to_fix.titleId), "couldn't get content count");
		TTRY(FixTicket(fulltik, new_tik_size, tik_hdr, num_contents), "couldn't fix ticket");
		TTRY(AMNET_InstallTicketBegin(&tik_import.Get()), "couldn't begin ticket installation");
		TTRY(FSFILE_Write(tik_import, &written, 0, fulltik, new_tik_size, 0), "couldn't write to ticket handle");
		TTRY(AMNET_InstallTicketFinish(tik_import), "couldn't finish ticket install");

		prog->update(i + 1, tiks_to_fix.size());
		rq.render_frame();
	}

	prog->set_hidden(true);
	progdesc->set_text(str::dlc_fixer_done);
	rq.render_finite_button(KEY_A);
}

