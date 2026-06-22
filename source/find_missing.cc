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

#include "find_missing.hh"
#include "hsapi.hh"
#include "mng.hh"
#include "queue.hh"
#include "panic.hh"
#include "log.hh"

#include <ui/loading.hh>

#include <algorithm>

static void list_installed(std::vector<ctr::title_id>& out)
{
	panic_if_err_3ds(ctr::mng::list_titles_on(MEDIATYPE_SD, out));
	ctr::mng::list_titles_on(MEDIATYPE_GAME_CARD, out); // it might error if there is no cart inserted so we don't want to panic if it fails
}

static void determine_local_missing(std::vector<ctr::title_id> &installed, std::vector<hsapi::RelatedFullTitle>& potentialInstalls, size_t& amount_found)
{
	amount_found = 0;
	std::vector<hsapi::RelatedFullTitle> newInstalls;
	std::copy_if(potentialInstalls.begin(), potentialInstalls.end(), std::back_inserter(newInstalls), [installed](const hsapi::RelatedFullTitle& title) -> bool {
		/* skip demo titles and base titles */
		if(title.relation == hsapi::TitleRelationType::Demo || title.relation == hsapi::TitleRelationType::Base) {
			dlog("skipping id %d tid %016llX because it's a demo or base", title.id, title.tid);
			return false;
		}

		/* skip titles already present in the queue */
		if(std::find_if(queue_get().begin(), queue_get().end(), [&title](const hsapi::Title& it) -> bool { return title.id == it.id; }) != queue_get().end()) {
			dlog("skipping id %d tid %016llX because it's already in the queue", title.id, title.tid);
			return false;
		}

		/* include titles that are actually missing */
		if(std::find(installed.begin(), installed.end(), title.tid) == installed.end()) {
			dlog("adding id %d tid %016llX because it's missing completely", title.id, title.tid);
			return true;
		}

		/* if they are not missing, check if they are up to date in terms of title version */
		AM_TitleEntry te;
		if(R_FAILED(ctr::mng::get_title_entry(title.tid, te)))
			return false;

		/* include titles with outdated title versions */
		if (title.version > te.version) {
			dlog("adding id %d tid %016llX because the installed one is older (v%d) compared to v%d", title.id, title.tid, te.version, title.version);
			return true;
		}
		dlog("skipping id %d tid %016llX because the installed one is newer (v%d) compared to v%d", title.id, title.tid, te.version, title.version);
		return false;
	});

	/* add included titles to the queue */
	for (const hsapi::Title& title : newInstalls)
		queue_add(title);

	amount_found = newInstalls.size();
}

void manual_find_missing(std::vector<hsapi::RelatedFullTitle>& potentialInstalls, size_t& amount_found)
{
	amount_found = 0;

	std::vector<ctr::title_id> installed;
	list_installed(installed);

	determine_local_missing(installed, potentialInstalls, amount_found);
}

Result show_find_missing(size_t& amount_found, hsapi::hid id)
{
	Result ret = 0;
	amount_found = 0;

	ui::loading([&id, &ret, &amount_found]() -> void {
		/* populated in both cases */
		std::vector<hsapi::RelatedFullTitle> potentialInstalls;

		std::vector<hsapi::htid> installed, installed_nand;
		list_installed(installed);
		panic_if_err_3ds(ctr::mng::list_titles_on(MEDIATYPE_NAND, installed_nand)); // mostly for streetpass dlc

		/* scan the whole system for missing titles */
		if (id == 0) {
			if (installed.size() == 0) /* should really never happen */
				return;

			/* for sd/gamecard */

			/* step 1: get tid->id pair */
			std::vector<hsapi::IdPair> relationPairs;
			ret = hsapi::scall<std::vector<hsapi::IdPair>&, const std::vector<hsapi::htid>&>(hsapi::id_pair_by_title_id, relationPairs, installed);
			if (R_FAILED(ret)) return;

			std::vector<hsapi::hid> relationSourceIds;
			relationSourceIds.reserve(relationPairs.size());

			std::transform(relationPairs.begin(), relationPairs.end(), std::back_inserter(relationSourceIds), [](const hsapi::IdPair& pair) -> hsapi::hid { return pair.id; });

			/* step 2: query titles to see which ones we must skip relations for */
			std::vector<hsapi::Title> relationSourceTitles;
			ret = hsapi::scall<std::vector<hsapi::Title>&, const std::vector<hsapi::hid>&>(hsapi::get_by_ids, relationSourceTitles, relationSourceIds);
			if(R_FAILED(ret)) return;

			for (const hsapi::Title& relationSourceTitle : relationSourceTitles)
				if (relationSourceTitle.flags & hsapi::TitleFlag::skip_force_rel)
					relationSourceIds.erase(std::remove(relationSourceIds.begin(), relationSourceIds.end(), relationSourceTitle.id), relationSourceIds.end());

			/* step 3: use new relations api to get a) legacy relations b) include new force related titles */
			ret = hsapi::scall<std::vector<hsapi::RelatedFullTitle>&, const std::vector<hsapi::hid>&>(hsapi::multiple_relations, potentialInstalls, relationSourceIds);
			if(R_FAILED(ret)) return;

			/* for nand */
			std::vector<hsapi::Title> nand_related;
			// here we need to use the old relations API as the the servers most likely don't
			// include the base system titles.
			ret = hsapi::scall<std::vector<hsapi::Title>&, const std::vector<hsapi::htid>&>(hsapi::batch_related, nand_related, installed_nand);
			if(R_FAILED(ret)) return;

			for (const hsapi::Title& relt : nand_related)
			{
				hsapi::TitleRelationType rel;
				switch (relt.tid.content_category()) { /* we should only need these for systitles tbh */
					case ctr::title_id::category::update_title:
						rel = hsapi::TitleRelationType::Update;
						break;
					case ctr::title_id::category::dlc_title:
						rel = hsapi::TitleRelationType::DLC;
						break;
					default:
						rel = hsapi::TitleRelationType::Other;
				}
				potentialInstalls.emplace_back(relt, rel);
			}
		}
		/* scan only for the given ID using new relations api */
		else
		{
			ret = hsapi::scall(hsapi::single_relations, potentialInstalls, std::move(id));
		}

		determine_local_missing(installed, potentialInstalls, amount_found);
	});
	return ret;
}

void show_find_missing_all()
{
	size_t num_missing;
	Result res = show_find_missing(num_missing);
	if(R_FAILED(res))
	{
		error_container err = get_error(res);
		report_error(err, "Find missing");
		handle_error(err);
	}
	else if(num_missing == 0) ui::notice(str::found_0_missing);
	else ui::notice(PSTRING(found_missing, num_missing));
}

