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

#ifndef inc_game_hh
#define inc_game_hh

#include <functional>
#include <string>
#include <3ds.h>

#include "hsapi.hh"

#define CIA_HANDLE_INVALID UINT32_MAX

typedef std::function<void(u64 /* done */, u64 /* total */)> prog_func;
typedef std::function<Result(std::string&)> get_url_func;
static void default_prog_func(u64, u64) { }

static inline get_url_func makeurlwrap(const std::string& url)
{
	return [url](std::string& ret) -> Result {
		ret = url;
		return 0;
	};
}

namespace install
{
	struct NetworkBenchmarkResult {
		u64 bytes = 0;
		u64 elapsed_ms = 0;
		float average_mib_s = 0.0f;
		float peak_mib_s = 0.0f;
		u64 single_bytes = 0;
		u64 single_elapsed_ms = 0;
		float single_average_mib_s = 0.0f;
		float single_peak_mib_s = 0.0f;
		bool has_parallel = false;
		u8 parallel_connections = 0;
	};

	void global_abort();
	const char *network_benchmark_phase();

	Result net_cia(get_url_func get_url, ctr::title_id tid, prog_func prog = default_prog_func,
		bool reinstallable = false, bool hsapi_enabled = false, bool do_ver_check = true, bool dev_auth = false);
	Result hs_cia(const hsapi::Title& meta, prog_func prog = default_prog_func,
		bool reinstallable = false);
	Result hs_network_benchmark(const hsapi::Title& meta, NetworkBenchmarkResult& result,
		prog_func prog = default_prog_func);

	inline bool is_in_progress()
	{
		extern Handle active_cia_handle;
		return active_cia_handle != CIA_HANDLE_INVALID;
	}

	/* Pre-fetch and cache the CDN download URL while the user browses.
	 * Called from the title details screen before the user presses install. */
	void pre_fetch_url(const hsapi::Title& meta);
	void clear_prefetched_url();
}

#endif
