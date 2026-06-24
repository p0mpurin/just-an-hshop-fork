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

#include "httpclient.hh"
#include "dmn.hh"
#include "hsapi.hh"
#include "nblib/nblib/nb/single_object.hh"
#include "nblib/nblib/objects/result.hh"
#include "update.hh" /* includes net constants like USER_AGENT */
#include "proxy.hh"
#include "error.hh"
#include "panic.hh"
#include "log.hh"
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include "build/hscert_der.h"

#ifdef RELEASE
	#include <ui/base.hh>
#endif

#define MIN(a,b) ((a)<(b)?(a):(b))

#define TRYJ( expr ) if(R_FAILED(res = ( expr ))) goto fail
#define HTTP_MAX_REDIRECT 10

extern "C" void        hsapi_password(char *); /* hsapi_auth.c */
extern "C" const int   hsapi_password_length;  /* hsapi_auth.c */
extern "C" const char *hsapi_user;             /* hsapi_auth.c */

static http::ResumableDownload *current_download = nullptr;
static LightLock current_download_lock;
static u32 hscert_chain = 0;

/* Store the last failed step for the updater UI */
static char g_last_http_error[256] = {};
namespace http {
	const char *http_last_error() { return g_last_http_error; }
}

static bool battery_is_critical()
{
	u8 level, state;
	PTMU_GetBatteryChargeState(&state);
	/* if we're charging we're not in critical condition */
	if(state) return false;
	PTMU_GetBatteryLevel(&level);
	/* i.e. critical for 5~0% */
	return level <= 1;
}

static const char *method_string_from_enum(HTTPC_RequestMethod method)
{
	switch(method)
	{
	case HTTPC_METHOD_GET: return "GET";
	case HTTPC_METHOD_POST: return "POST";
	case HTTPC_METHOD_HEAD: return "HEAD";
	case HTTPC_METHOD_PUT: return "PUT";
	case HTTPC_METHOD_DELETE: return "DELETE";
	}
	return "<unknown>";
}

#if 0
static const char encodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64Encode(u8 *inbuf, u32 size)
{
	std::string encodedString;
	encodedString.reserve(((size / 3) + (size % 3 > 0)) * 4);
	u32 temp;
	for(size_t idx = 0; idx < size / 3; idx++)
	{
		temp  = (*inbuf++) << 16; //Convert to big endian
		temp += (*inbuf++) << 8;
		temp += (*inbuf++);
		encodedString.append(1, encodeLookup[(temp & 0x00FC0000) >> 18]);
		encodedString.append(1, encodeLookup[(temp & 0x0003F000) >> 12]);
		encodedString.append(1, encodeLookup[(temp & 0x00000FC0) >> 6 ]);
		encodedString.append(1, encodeLookup[(temp & 0x0000003F)      ]);
	}
	switch(size % 3)
	{
	case 1:
		temp  = (*inbuf++) << 16; //Convert to big endian
		encodedString.append(1, encodeLookup[(temp & 0x00FC0000) >> 18]);
		encodedString.append(1, encodeLookup[(temp & 0x0003F000) >> 12]);
		encodedString.append(2, '=');
		break;
	case 2:
		temp  = (*inbuf++) << 16; //Convert to big endian
		temp += (*inbuf++) << 8;
		encodedString.append(1, encodeLookup[(temp & 0x00FC0000) >> 18]);
		encodedString.append(1, encodeLookup[(temp & 0x0003F000) >> 12]);
		encodedString.append(1, encodeLookup[(temp & 0x00000FC0) >> 6 ]);
		encodedString.append(1, '=');
		break;
	}
	return encodedString;
}

static Result get_console_info(std::string &out_ctcert_hash, std::string &out_ctcert_sig, std::string &out_device_id) {
	return 0;
}
#endif

void http::ResumableDownload::global_abort()
{
	for(http::ResumableDownload *dl = current_download; dl; dl = dl->next)
		dl->abort_and_close();
	ResumableDownload::backend_global_deinit();
}

http::ResumableDownload::ResumableDownload(bool hsapi_enabled, bool version_check_enabled, bool dev_auth)
{
	this->hsapiEnabled = hsapi_enabled;
	this->versionCheckEnabled = version_check_enabled;
	if (dev_auth) this->requires_device_authentication();

	/* Page alignment reduces IPC cache/buffer friction for the large receive
	 * blocks used by the optimized install pipeline. */
	this->buffer = memalign(0x1000, http::ResumableDownload::ChunkMaxSize);
	panic_assert(this->buffer, "failed to allocate for download buffer");

	static bool lock_is_init = false;
	if(!lock_is_init)
	{
		LightLock_Init(&current_download_lock);
		lock_is_init = true;
	}
	ctr::LockedInScope slock { &current_download_lock };

	if(current_download)
	{
		this->next = current_download->next;
		this->prev = current_download;
		current_download->next = this;
	}
	else
	{
		this->next = this->prev = nullptr;
		current_download = this;
	}
}

http::ResumableDownload::~ResumableDownload()
{
	free(this->buffer);

	ctr::LockedInScope slock { &current_download_lock };

	if(this->next) this->next->prev = this->prev;
	if(this->prev) this->prev->next = this->next;
	if(current_download == this)
	{
		panic_assert(!this->prev, "invalid state");
		current_download = this->next;
	}

	this->abort_and_close();
}

void http::ResumableDownload::abort_and_close()
{
	if(!this->in_progress())
		return;
	this->abort();
	while(this->in_progress())
		usleep(100000); /* 0.1 second */
	this->close_handle();
}

void http::ResumableDownload::notify()
{
	if(this->notify_event)
		this->notify_event->signal();
}

Result http::ResumableDownload::execute_once()
{
	/* at this point we cannot execute anymore */
	if(this->flags & http::ResumableDownload::flag_exit)
		return APPERR_CANCELLED;

	/* we require the sleep lock for downloads and such, lets just ensure it's available */
	Result res = ctr::dmn::increase_sleep_lock_ref();
	if(R_FAILED(res)) elog("failed to acquire sleep lock: %08lX", res);
	vlog("Trying to download URL '%s'...", this->url.c_str());
	res = this->perform_execute_once(this->url.c_str(), 0);
	ctr::dmn::decrease_sleep_lock_ref();
	return res;
}

#if HTTP_BACKEND == HTTP_BACKEND_HTTPC
void http::ResumableDownload::close_handle()
{
	if(!(this->flags & http::ResumableDownload::flag_exit))
		httpcCancelConnection(&this->hctx);
	httpcCloseContext(&this->hctx);
}

Result http::ResumableDownload::perform_execute_once(const char *url, int redirection_depth)
{
	u32 pos = 0, prev_pos = 0;
	size_t chunk_size = 0;
	Result res = 0, nres = 0;
	s32 status = 0;
	u32 chunk_num = 0;
	u32 request_size = 0;
	const char *fail_stage = "start";
	bool bad_http_status = false;

	if(battery_is_critical())
		return APPERR_CRITICAL_BAT;

	ilog("[http] setup_handle url=%s", url);
	if(R_FAILED(res = this->setup_handle(url)))
	{
		elog("[http] setup_handle FAILED 0x%08lX", res);
		return res;
	}
	ilog("[http] setup_handle OK");

	fail_stage = "begin";
	ilog("[http] httpcBeginRequest");
	TRYJ(httpcBeginRequest(&this->hctx));
	ilog("[http] httpcBeginRequest OK");

	fail_stage = "status";
	ilog("[http] httpcGetResponseStatusCode");
	TRYJ(httpcGetResponseStatusCodeTimeout(&this->hctx, (u32 *)&status, this->timeout));
	ilog("[http] status=%ld", status);

	#if defined(EMULATOR) || defined(PRERELEASE)
	if (status < 0) {
		res = 0xD8A0A049;
		goto fail;
	}
	#endif

	vlog("Status code on URL %s '%s' is %ld.", method_string_from_enum(this->method), url, status);

	/* we may need to redirect (status code 3xx) */
	if(status / 100 == 3)
	{
		if(redirection_depth == HTTP_MAX_REDIRECT)
		{
			elog("Reached maximum amount of redirects (%d)", HTTP_MAX_REDIRECT);
			return APPERR_MAX_REDIRECTS;
		}

		char newurl[4096];
		fail_stage = "redirect";
		TRYJ(httpcGetResponseHeader(&this->hctx, "location", newurl, sizeof(newurl)));
		newurl[sizeof(newurl) - 1] = '\0';

		this->close_handle();
		vlog("Following redirect to %s", newurl);
		return this->perform_execute_once(newurl, redirection_depth + 1);
	}

	if((this->downloadedSize == 0 && status != 200) || (this->downloadedSize != 0 && status != 206))
	{
		elog("Status code indicating failure, expected %d, got %ld", this->downloadedSize == 0 ? 200 : 206, status);
		if (this->downloadedSize == 0 && status == 413) {
			res = APPERR_TOO_LARGE;
			goto fail;
		}
		bad_http_status = true;
	}

#ifdef RELEASE
	// We _may_ require a different 3hs version
	if (this->versionCheckEnabled) { /* the idea is to made the version check optional because else updateserver:/3hs/version will fail... */
		char buffer[33] = { 0 };
		update::app_version server_version;
		Result header_res = httpcGetResponseHeader(&this->hctx, "x-minimum", buffer, sizeof(buffer) - 1);
		bool valid_version = R_SUCCEEDED(header_res)
			&& server_version.parse(buffer, sizeof(buffer));

		/* x-minimum is a compatibility hint, not part of the API payload.
		 * Missing/malformed hints must not brick a custom client. */
		if(!valid_version)
		{
			ilog("API did not provide a valid x-minimum header; continuing");
		}
		else if(server_version > update::CUR_APP_VERSION)
		{
			this->flags &= ~(http::ResumableDownload::flag_active);
			this->close_handle();
			ui::RenderQueue::terminate_render();
			ilog("current app version %s is lower than API reported version %s", VERSION, buffer);
			ui::notice(PSTRING(min_constraint, VVERSION, buffer));
			exit(1);
		}

		if(valid_version)
			dlog("received app version %d.%d.%d from api", server_version.maj, server_version.min, server_version.patch);
	}
#endif

	if(!(this->flags & http::ResumableDownload::flag_size))
	{
		fail_stage = "size";
		TRYJ(httpcGetDownloadSizeState(&this->hctx, nullptr, &this->totalSize));
		if(R_FAILED(res = this->on_total_size_try_get_()))
			goto fail;
		this->notify();
		this->flags |= http::ResumableDownload::flag_size;
	}

	panic_assert(this->buffer, "buffer should be allocated");


	do {
		if(battery_is_critical())
		{
			res = APPERR_CRITICAL_BAT;
			goto fail;
		}
		if(this->flags & http::ResumableDownload::flag_exit) goto cancel;
		request_size = http::ResumableDownload::ChunkMaxSize;
		if(this->totalSize > prev_pos)
			request_size = MIN(request_size, this->totalSize - prev_pos);
		fail_stage = "recv";
		res = httpcReceiveDataTimeout(&this->hctx, (u8 *) this->buffer, request_size, this->timeout);
		if(res) ilog("[http] httpcReceiveData returned 0x%08lX chunk=%lu", res, (unsigned long)chunk_num);
		if(this->flags & http::ResumableDownload::flag_exit) goto cancel;


		fail_stage = "progress";
		nres = httpcGetDownloadSizeState(&this->hctx, &pos, nullptr);
		if(R_FAILED(nres)) res = nres;
		if((R_FAILED(res) && res != (Result) HTTPC_RESULTCODE_DOWNLOADPENDING))
		{
			/* Some servers close the connection after sending the full
			 * response body. If we already have all the expected data,
			 * treat that as a clean end-of-stream. */
			if(this->totalSize > 0 && prev_pos >= this->totalSize)
			{
				ilog("[http] connection closed after complete download (%lu/%lu bytes)",
					(unsigned long)prev_pos, (unsigned long)this->totalSize);
				res = 0;
				break;
			}
			elog("aborted http connection due to error: %08lX.", res);
			goto fail;
		}
		if(res == (Result) HTTPC_RESULTCODE_DOWNLOADPENDING)
		{
			if(pos != prev_pos + http::ResumableDownload::ChunkMaxSize)
			{
				/* Partial chunk is fine — the HTTPC service may deliver
				 * less than the requested ChunkMaxSize. Accept any progress
				 * and request the next chunk. */
			}
		}
		chunk_size = pos - prev_pos;
		if (this->hsapiEnabled && chunk_num == 0 && status != 200 && status != 206) {
			nb::Result nbres;
			if (nb::single_object::parse<nb::Result>(nbres, (u8 *)this->buffer, pos) == nb::StatusCode::SUCCESS) {
				res = hsapi::handle_nb_result(nbres);
				goto fail;
			}
			/* Do not pass an HTML/proxy/CDN error page to an NB parser. */
			res = APPERR_NON200;
			goto fail;
		}
		nres = this->on_chunk_(chunk_size);
		/* we can't just assign res = nres becaues res may be either
		 * HTTPC_RESULTCODE_DOWNLOADPENDING or 0, which we can't discard */
		if(R_FAILED(nres)) res = nres;
		this->downloadedSize += chunk_size;
		prev_pos = pos;
		++chunk_num;
		/* Publish progress only after the byte count reflects this chunk. */
		this->notify();

		if(this->totalSize > 0 && prev_pos >= this->totalSize)
		{
			res = 0;
			break;
		}
	} while(res == (Result) HTTPC_RESULTCODE_DOWNLOADPENDING);

	if(bad_http_status)
		res = APPERR_NON200;
	if(res == 0) this->notify();

fail:
	elog("[http] FAIL stage=%s chunk=%lu code=0x%08lX status=%ld pos=%lu prev=%lu total=%lu req=%lu url=%s",
		fail_stage, (unsigned long)chunk_num, res, status, (unsigned long)pos,
		(unsigned long)prev_pos, (unsigned long)this->totalSize,
		(unsigned long)request_size, url);
	snprintf(g_last_http_error, sizeof(g_last_http_error),
		"%s c%lu r=0x%08lX st=%ld pos=%lu/%lu prev=%lu req=%lu",
		fail_stage, (unsigned long)chunk_num, res, status,
		(unsigned long)pos, (unsigned long)this->totalSize,
		(unsigned long)prev_pos, (unsigned long)request_size);
	this->close_handle();
	this->flags &= ~(http::ResumableDownload::flag_active | http::ResumableDownload::flag_exit);
	return res;
cancel:
	res = APPERR_CANCELLED;
	goto fail;
}

Result http::ResumableDownload::setup_handle(const char *url)
{
	char *password;
	Result res;
	const char *fail_stage = "open";

	ilog("[http] setup_handle url=%s", url);
	/* the last argument is use_default_proxy, we don't want that since we set it ourselves later */
	if(R_FAILED(res = httpcOpenContext(&this->hctx, this->method, url, 0)))
	{
		elog("[http] httpcOpenContext FAIL 0x%08lX", res);
		return res;
	}
	ilog("[http] httpcOpenContext OK");

	/* Explicitly retain the connection for the duration of large transfers. */
	fail_stage = "keepalive";
	ilog("[http] httpcSetKeepAlive");
	TRYJ(httpcSetKeepAlive(&this->hctx, HTTPC_KEEPALIVE_ENABLED));
	ilog("[http] httpcSetKeepAlive OK");
	
	ilog("[http] ssl setup (is_https=%d)", strncmp(url, "https:", 6) == 0);
	if(hscert_der_bin_size && strncmp(url, "https:", 6) == 0
		&& (this->flags & (flag_auth | flag_device_auth)))
	{
		fail_stage = "cert";
		TRYJ(httpcSelectRootCertChain(&this->hctx, hscert_chain));
	}
	
	if(strncmp(url, "https:", 6) == 0)
	{
		fail_stage = "sslopt";
		TRYJ(httpcSetSSLOpt(&this->hctx, SSLCOPT_DisableVerify));
	}
	ilog("[http] ssl setup OK");
	ilog("[http] adding headers...");
	fail_stage = "ua";
	TRYJ(httpcAddRequestHeaderField(&this->hctx, "User-Agent", USER_AGENT));
	ilog("[http] User-Agent added");
	if(this->flags & http::ResumableDownload::flag_auth)
	{
		fail_stage = "auth-user";
		TRYJ(httpcAddRequestHeaderField(&this->hctx, "X-Auth-User", hsapi_user));
		ilog("[http] X-Auth-User added");
		fail_stage = "auth-pass";
		/*TRYJ(httpcAddRequestHeaderField(&this->hctx, "X-Auth-Password", password));*/password=(char*)malloc(hsapi_password_length+1);hsapi_password(password);password[hsapi_password_length]=0;TRYJ(httpcAddRequestHeaderField(&this->hctx,"X-Auth-Password",password));memset(password,0,hsapi_password_length);free(password);
		ilog("[http] X-Auth-Password added");
	}
#if 0
	if(this->flags & http::ResumableDownload::flag_device_auth)
	{
	}
#endif
	if(this->postdataPtr && this->postdataLen)
	{
		fail_stage = "post";
		TRYJ(httpcAddPostDataRaw(&this->hctx, (const u32 *) this->postdataPtr, this->postdataLen));
	}
	/* we start from downloadedSize; 0 initially and it increments after the second execute_once() */
	if(this->downloadedSize != 0)
	{
		std::string val = "bytes=" + std::to_string(this->downloadedSize) + "-";
		fail_stage = "range";
		TRYJ(httpcAddRequestHeaderField(&this->hctx, "Range", val.c_str()));
		ilog("[http] Range header added");
	}
	ilog("[http] proxy::apply...");
	fail_stage = "proxy";
	TRYJ(proxy::apply(&this->hctx));
	ilog("[http] proxy::apply OK");

	this->flags |= http::ResumableDownload::flag_active;
	ilog("[http] setup_handle SUCCESS");
	return 0;
fail:
	elog("[http] setup_handle FAIL stage=%s code=0x%08lX url=%s", fail_stage, res, url);
	snprintf(g_last_http_error, sizeof(g_last_http_error), "setup:%s 0x%08lX", fail_stage, res);
	this->close_handle();
	return res;
}

void http::ResumableDownload::abort()
{
	this->flags |= http::ResumableDownload::flag_exit;
	if(this->flags & http::ResumableDownload::flag_active)
		httpcCancelConnection(&this->hctx);
}

Result http::ResumableDownload::global_init()
{
	/* A larger HTTPC shared-memory pool gives the service room to keep the
	 * receive side fed while 1 MiB blocks are handed to the application. */
	Result res = httpcInit(4 * 1024 * 1024);
	if (R_FAILED(res)) return res;
	
	if (hscert_der_bin_size == 0) {
		return 0;
	}
	
	res = httpcCreateRootCertChain(&hscert_chain);
	if (R_FAILED(res)) return res;
	
	return httpcRootCertChainAddCert(hscert_chain, hscert_der_bin, hscert_der_bin_size, NULL);
}

void http::ResumableDownload::backend_global_deinit()
{
	httpcExit();
}

#elif HTTP_BACKEND == HTTP_BACKEND_CURL
bool http::ResumableDownload::append_header(const char *key, const char *value)
{
	return this->append_header((std::string(key) + ": " + value).c_str());
}

bool http::ResumableDownload::append_header(const char *line)
{
	struct curl_slist *nsl = curl_slist_append(this->headers, line);
	if(!nsl) return false;
	this->headers = nsl;
	return true;
}

void http::ResumableDownload::close_handle()
{
	curl_easy_cleanup(this->handle);
	curl_slist_free_all(this->headers);
}

size_t http::ResumableDownload::http_curl_write_callback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	panic_assert(size == 1, "inval");
	size = nmemb;

	http::ResumableDownload *download = (http::ResumableDownload *) userdata;
	while(size)
	{
		u32 current_fill  = download->downloadedSize % http::ResumableDownload::ChunkMaxSize;
		u32 left_in_chunk = http::ResumableDownload::ChunkMaxSize - current_fill;
		u32 to_copy = MIN(size, left_in_chunk);
		memcpy(&((u8 *) download->buffer)[current_fill], ptr, to_copy);

		/* We have a block */
		if(current_fill + to_copy == http::ResumableDownload::ChunkMaxSize)
		{
			download->last_result = download->on_chunk_(http::ResumableDownload::ChunkMaxSize);
		}

		download->downloadedSize += to_copy;
		ptr += to_copy;
		size -= to_copy;

		/* Signal because download->downloadedSize updated */
		download->notify();
		if(download->last_result != 0)
			return 0;
	}

	return nmemb;
}

int http::ResumableDownload::http_curl_progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	(void) ulnow;
	(void) ultotal;
	(void) dlnow;

	http::ResumableDownload *download = (http::ResumableDownload *) clientp;
	if(!(download->flags & http::ResumableDownload::flag_size))
	{
		download->totalSize = dltotal;
		if(R_FAILED(download->last_result = download->on_total_size_try_get_()))
			return 1;
		download->notify();
		download->flags |= http::ResumableDownload::flag_size;
	}

	return download->flags & http::ResumableDownload::flag_exit;
}

Result http::ResumableDownload::perform_execute_once(const char *url, int redirection_depth)
{
	Result res;

	if(battery_is_critical())
		return APPERR_CRITICAL_BAT;

	if(R_FAILED(res = this->setup_handle(url)))
		return res;

	CURLcode cres = curl_easy_perform(this->handle);

	dlog("Completed execute_once, last_result = 0x%08lX", this->last_result);

	if(this->last_result == 0 && cres != CURLE_OK)
		this->last_result = APPERR_FAILED_CURL;
	if(this->last_result == 0)
	{
		u32 current_fill = this->downloadedSize % http::ResumableDownload::ChunkMaxSize;
		/* Write the last chunk */
		if(current_fill)
			this->on_chunk_(current_fill);
		this->notify();
	}

	this->close_handle();
	this->flags &= ~(http::ResumableDownload::flag_active | http::ResumableDownload::flag_exit);
	return this->last_result;
}

Result http::ResumableDownload::setup_handle(const char *url)
{
	this->handle = curl_easy_init();
	if(!this->handle) return APPERR_FAILED_CURL;

	if(!this->append_header("User-Agent: " USER_AGENT))
		goto fail;

	curl_easy_setopt(this->handle, CURLOPT_URL, url);
	/* We need to add a POST body */
	if(this->method == HTTPC_METHOD_POST)
	{
		panic_assert(this->postdataPtr && this->postdataLen, "must provide postdata ptr and len for POST request");
		curl_easy_setopt(this->handle, CURLOPT_POSTFIELDSIZE, this->postdataLen);
		curl_easy_setopt(this->handle, CURLOPT_POSTFIELDS, this->postdataPtr);
		curl_easy_setopt(this->handle, CURLOPT_POST, 1L);
	}
	/* For anything that's not GET otherwise we need to
	 * set it, GET is default and therefore no extra work required */
	else if(this->method != HTTPC_METHOD_GET)
	{
		curl_easy_setopt(this->handle, CURLOPT_CUSTOMREQUEST, method_string_from_enum(this->method));
	}

	curl_easy_setopt(this->handle, CURLOPT_TIMEOUT, this->timeout / 1000000000L);
	curl_easy_setopt(this->handle, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(this->handle, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(this->handle, CURLOPT_MAXREDIRS, 5L);
	curl_easy_setopt(this->handle, CURLOPT_NOPROGRESS, 0L);

	curl_easy_setopt(this->handle, CURLOPT_XFERINFOFUNCTION, http::ResumableDownload::http_curl_progress_callback);
	curl_easy_setopt(this->handle, CURLOPT_WRITEFUNCTION, http::ResumableDownload::http_curl_write_callback);

	curl_easy_setopt(this->handle, CURLOPT_XFERINFODATA, this);
	curl_easy_setopt(this->handle, CURLOPT_WRITEDATA, this);

	if(this->downloadedSize != 0)
	{
		std::string val = "bytes=" + std::to_string(this->downloadedSize) + "-";
		if(!this->append_header("Range", val.c_str()))
			goto fail;
	}

	if(this->flags & http::ResumableDownload::flag_auth)
	{
		char *password;
		if(!this->append_header("X-Auth-User", hsapi_user)) goto fail;
		/*TRYJ(httpcAddRequestHeaderField(&this->hctx, "X-Auth-Password", password));*/password=(char*)malloc(hsapi_password_length+1);hsapi_password(password);password[hsapi_password_length]=0;if(!this->append_header("X-Auth-Password",password))goto fail;memset(password,0,hsapi_password_length);free(password);
	}

	panic_assert(!get_nsettings()->proxy_port, "Proxy not supported in curl backend!");

	/* Set the headers */
	curl_easy_setopt(handle, CURLOPT_HTTPHEADER, this->headers);

	this->flags |= http::ResumableDownload::flag_active;
	return 0;
fail:
	this->close_handle();
	return APPERR_FAILED_CURL;
}

void http::ResumableDownload::abort()
{
	this->flags |= http::ResumableDownload::flag_exit;
}

Result http::ResumableDownload::global_init()
{
	return curl_global_init(CURL_GLOBAL_ALL) == 0
		? 0 : APPERR_FAILED_CURL;
}

void http::ResumableDownload::backend_global_deinit()
{
	curl_global_cleanup();
}

#else
	#error "HTTP_BACKEND has an improper value!"
#endif
