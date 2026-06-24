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

#ifndef inc_httpclient_hh
#define inc_httpclient_hh

#include "sync.hh"
#include <functional>
#include <string>
#include <3ds.h>

#ifndef HTTP_BACKEND
	#define HTTP_BACKEND HTTP_BACKEND_HTTPC
#endif

#define HTTP_BACKEND_HTTPC 0
#define HTTP_BACKEND_CURL  1

#if HTTP_BACKEND == HTTP_BACKEND_CURL
	#include <curl/curl.h>
#endif

namespace http
{
	const char *http_last_error();
	class ResumableDownload
	{
	public:
		static constexpr u64 DefaultTimeout = 10000000000L; /* 10 seconds */
		/* Large enough to amortize HTTP IPC and AM write overhead while
		 * retaining responsive cancellation on New 3DS hardware. */
		static constexpr size_t ChunkMaxSize = 0x100000; /* 1 MiB */

		static Result global_init();
		static void global_abort();

		ResumableDownload(bool hsapi_enabled = false, bool version_check_enabled = true, bool dev_auth = false);
		~ResumableDownload();

		/* Tries to execute the download, once, when called
		 * again it continues the download if there was still
		 * data left to download
		 */
		Result execute_once();

		void abort();

		void on_total_size_try_get(std::function<Result()> func) { this->on_total_size_try_get_ = func; }
		void requires_authentication() { this->flags |= http::ResumableDownload::flag_auth; }
		void requires_device_authentication() { this->flags |= http::ResumableDownload::flag_device_auth; }
		void on_chunk(std::function<Result(size_t)> func) { this->on_chunk_ = func; }

		/* Event that is signaled when the state changes, it is the equivalent of
		 * signalling the event in both on_chunk() and on_total_size_try_get(), additionally
		 * it also signals when the download is completed
		 * Note that this event is signaled *after* the callbacks are called */
		void set_notify_event(ctr::Event *event) { this->notify_event = event; }
		/* 0 if total size is not known (yet), after on_total_size_try_get() it is known
		 * if the total size is known */
		u32 maybe_total_size() { return this->totalSize; }
		/* returns the amount of bytes downloaded since the creation of this object
		 * note that this count is only increment *after* on_chunk() is called and the
		 * notification event is signaled */
		u32 downloaded() { return this->downloadedSize; }
		/* returns the temporary buffer where data is downloaded in, should be used
		 * in on_chunk(size_t buffer_size), the parameter is the usable byte count in
		 * the buffer returned by data_buffer() */
		template <typename T = u8>
		const T *data_buffer() { return (T *) this->buffer; }

		/* the data pointer must remain valid as long as the ResumableDownload is */
		void set_postdata(const void *data, u32 len)
		{
			this->postdataPtr = data;
			this->postdataLen = len;
		}

		void set_target(const std::string& url, HTTPC_RequestMethod method)
		{
			this->url = url;
			this->method = method;
		}

		void set_timeout(u64 ntm)
		{
			this->timeout = ntm;
		}

		bool in_progress()
		{
			return this->flags & http::ResumableDownload::flag_active;
		}

	private:
		std::function<Result()> on_total_size_try_get_ = []() -> Result { return 0; };
		std::function<Result(size_t)> on_chunk_ = [](size_t) -> Result { return 0; };

		u64 timeout = DefaultTimeout;

		void *buffer;

		u32 totalSize = 0, downloadedSize = 0;

		HTTPC_RequestMethod method;
		std::string url;

		ctr::Event *notify_event = nullptr;

		Result perform_execute_once(const char *url, int redirection_depth);
		Result setup_handle(const char *url);
		static void backend_global_deinit();
		void abort_and_close();
		void close_handle();
		void notify();

		const void *postdataPtr = nullptr;
		u32 postdataLen = 0;
		bool hsapiEnabled = false;
		bool versionCheckEnabled = false;

#if HTTP_BACKEND == HTTP_BACKEND_HTTPC
		httpcContext hctx;
#elif HTTP_BACKEND == HTTP_BACKEND_CURL
		static int http_curl_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
		static size_t http_curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata);

		bool append_header(const char *key, const char *value);
		bool append_header(const char *line);

		struct curl_slist *headers = nullptr;
		Result last_result = 0;
		CURL *handle;
#else
	#error "HTTP_BACKEND has an improper value!"
#endif

		enum {
			flag_active      = 1,  /* transfer is in progress */
			flag_exit        = 2,  /* transfer needs to be cancelled */
			flag_auth        = 4,  /* transfer is authenticated */
			flag_size        = 8,  /* total size was gotten */
			flag_device_auth = 16, /* transfer is authenticated with device info */
		}; int flags = 0;

		/* linked list for the global current downloads list */
		http::ResumableDownload *next, *prev;
	};
}

#endif
