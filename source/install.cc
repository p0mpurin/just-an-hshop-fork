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
#include "mng.hh"
#include "settings.hh"
#include "install.hh"
#include "thread.hh"
#include "error.hh"
#include "panic.hh"
#include "log.hh"

#include <3ds.h>
#include <malloc.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <algorithm>
#include <cctype>

namespace ui
{
	void scan_keys();
	u32 kDown();
	u32 kHeld();
}

using expandable_binary_data_type = std::basic_string<u8>;

namespace install {
	Handle active_cia_handle = CIA_HANDLE_INVALID;
	static bool direct_cdn_active = false;
	bool is_direct_cdn_active() { return direct_cdn_active; }
}
using install::active_cia_handle;

/* defined in file_fwd.cc */
Result install_forwarder(u8 *data, size_t len);

enum class ThreadState {
	Installing,
	Timeout,
	Abort,
	Finished,
};

class DirectCdnDownload
{
public:
	DirectCdnDownload(bool, bool, bool)
	{
		this->buffer = (u8 *)memalign(0x1000, receive_size);
	}
	~DirectCdnDownload() { free(this->buffer); }

	void set_notify_event(ctr::Event *event) { this->notify_event = event; }
	void set_target(const std::string& target, HTTPC_RequestMethod) { this->url = target; }
	void on_total_size_try_get(std::function<Result()> func) { this->on_total = func; }
	void on_chunk(std::function<Result(size_t)> func) { this->on_chunk_cb = func; }
	u32 downloaded() const { return this->downloaded_size; }
	u32 maybe_total_size() const { return this->total_size; }
	const u8 *data_buffer() const { return this->buffer; }

	void abort()
	{
		this->cancelled = true;
		int fd = this->socket_fd;
		if(fd >= 0)
			shutdown(fd, SHUT_RDWR);
	}

	Result execute_once()
	{
		return this->execute_url(this->url, 0);
	}

private:
	Result execute_url(const std::string& target_url, unsigned redirect_depth)
	{
		std::string host, path;
		if(redirect_depth > 5 || !this->parse_url(target_url, host, path))
			return APPERR_DIRECT_SOCKET_SETUP;
		if(!this->buffer)
			return APPERR_OUT_OF_MEM;

		struct addrinfo hints {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		struct addrinfo *addresses = nullptr;
		if(getaddrinfo(host.c_str(), "80", &hints, &addresses) != 0)
			return APPERR_DIRECT_SOCKET_SETUP;

		Result result = APPERR_DIRECT_SOCKET_SETUP;
		for(struct addrinfo *it = addresses; it && this->socket_fd < 0; it = it->ai_next)
		{
			int fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
			if(fd < 0)
				continue;
			int receive_buffer = 1024 * 1024;
			setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &receive_buffer, sizeof(receive_buffer));
			int original_flags = fcntl(fd, F_GETFL, 0);
			fcntl(fd, F_SETFL, original_flags | O_NONBLOCK);
			int connected = connect(fd, it->ai_addr, it->ai_addrlen);
			if(connected != 0 && errno != EINPROGRESS)
			{
				close(fd);
				continue;
			}

			bool ready = connected == 0;
			for(unsigned waited_ms = 0; !ready && !this->cancelled && waited_ms < 10000; waited_ms += 100)
			{
				struct pollfd pfd { fd, POLLOUT, 0 };
				int poll_result = poll(&pfd, 1, 100);
				this->notify();
				if(poll_result > 0)
				{
					int socket_error = 0;
					socklen_t error_size = sizeof(socket_error);
					if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &error_size) == 0
						&& socket_error == 0)
						ready = true;
					else
						break;
				}
			}

			if(ready && !this->cancelled)
			{
				fcntl(fd, F_SETFL, original_flags);
				this->socket_fd = fd;
			}
			else
				close(fd);
		}
		freeaddrinfo(addresses);
		if(this->socket_fd < 0)
			return result;

		std::string request =
			"GET " + path + " HTTP/1.1\r\n"
			"Host: " + host + "\r\n"
			"User-Agent: Nocturne-Direct/1.0\r\n"
			"Accept: */*\r\n"
			"Accept-Encoding: identity\r\n"
			"Connection: close\r\n\r\n";
		size_t sent = 0;
		while(sent < request.size())
		{
			ssize_t count = send(this->socket_fd, request.data() + sent, request.size() - sent, 0);
			if(count <= 0)
				goto done;
			sent += count;
		}

		{
			std::string header;
			bool header_complete = false;
			result = APPERR_DIRECT_SOCKET_TRANSFER;

			while(!this->cancelled)
			{
				ssize_t count = recv(this->socket_fd, this->buffer, receive_size, 0);
				if(count <= 0)
					break;

				if(!header_complete)
				{
					header.append((const char *)this->buffer, count);
					size_t separator = header.find("\r\n\r\n");
					if(separator == std::string::npos)
					{
						if(header.size() > 16 * 1024)
							break;
						continue;
					}

					size_t header_size = separator + 4;
					std::string lower = header.substr(0, header_size);
					std::transform(lower.begin(), lower.end(), lower.begin(),
						[](unsigned char c) { return std::tolower(c); });
					bool redirect =
						lower.find("http/1.1 301") == 0 || lower.find("http/1.0 301") == 0 ||
						lower.find("http/1.1 302") == 0 || lower.find("http/1.0 302") == 0 ||
						lower.find("http/1.1 303") == 0 || lower.find("http/1.0 303") == 0 ||
						lower.find("http/1.1 307") == 0 || lower.find("http/1.0 307") == 0 ||
						lower.find("http/1.1 308") == 0 || lower.find("http/1.0 308") == 0;
					if(redirect)
					{
						size_t location_pos = lower.find("\r\nlocation:");
						if(location_pos == std::string::npos)
						{
							result = APPERR_DIRECT_SOCKET_SETUP;
							break;
						}
						location_pos += strlen("\r\nlocation:");
						while(location_pos < header_size && (header[location_pos] == ' ' || header[location_pos] == '\t'))
							++location_pos;
						size_t location_end = header.find("\r\n", location_pos);
						std::string redirect_url = header.substr(location_pos, location_end - location_pos);
						if(redirect_url.compare(0, 2, "//") == 0)
							redirect_url = "http:" + redirect_url;
						else if(!redirect_url.empty() && redirect_url[0] == '/')
							redirect_url = "http://" + host + redirect_url;

						close(this->socket_fd);
						this->socket_fd = -1;
						result = this->execute_url(redirect_url, redirect_depth + 1);
						return result;
					}
					if(lower.find("http/1.1 200") != 0 && lower.find("http/1.0 200") != 0)
					{
						result = APPERR_DIRECT_SOCKET_SETUP;
						break;
					}
					size_t length_pos = lower.find("\r\ncontent-length:");
					if(length_pos == std::string::npos)
					{
						result = APPERR_DIRECT_SOCKET_SETUP;
						break;
					}
					length_pos += strlen("\r\ncontent-length:");
					while(length_pos < lower.size() && lower[length_pos] == ' ')
						++length_pos;
					this->total_size = strtoul(lower.c_str() + length_pos, nullptr, 10);
					if(!this->total_size || R_FAILED(result = this->on_total()))
						break;
					this->notify();
					header_complete = true;

					size_t body_in_header = header.size() - header_size;
					if(body_in_header)
					{
						memcpy(this->buffer, header.data() + header_size, body_in_header);
						count = body_in_header;
					}
					else
						continue;
				}

				if(R_FAILED(result = this->on_chunk_cb((size_t)count)))
					break;
				this->downloaded_size += count;
				this->notify();
				if(this->downloaded_size >= this->total_size)
				{
					result = 0;
					break;
				}
			}
			if(this->cancelled)
				result = APPERR_CANCELLED;
			else if(header_complete && this->downloaded_size == this->total_size)
				result = 0;
			else if(this->downloaded_size == 0 && result == APPERR_DIRECT_SOCKET_TRANSFER)
				result = APPERR_DIRECT_SOCKET_SETUP;
		}

done:
		if(this->socket_fd >= 0)
			close(this->socket_fd);
		this->socket_fd = -1;
		return result;
	}

	bool parse_url(const std::string& target_url, std::string& host, std::string& path)
	{
		static const std::string prefix = "http://";
		if(target_url.compare(0, prefix.size(), prefix) != 0)
			return false;
		size_t slash = target_url.find('/', prefix.size());
		host = target_url.substr(prefix.size(), slash - prefix.size());
		path = slash == std::string::npos ? "/" : target_url.substr(slash);
		return !host.empty();
	}

	void notify()
	{
		if(this->notify_event)
			this->notify_event->signal();
	}

	std::string url;
	static constexpr size_t receive_size = 256 * 1024;
	u8 *buffer = nullptr;
	std::function<Result()> on_total = []() -> Result { return 0; };
	std::function<Result(size_t)> on_chunk_cb = [](size_t) -> Result { return 0; };
	ctr::Event *notify_event = nullptr;
	volatile bool cancelled = false;
	volatile int socket_fd = -1;
	u32 downloaded_size = 0;
	u32 total_size = 0;
};

template <typename Downloader>
struct thread_data {
	Downloader *downloader;
	get_url_func *get_url;
	ctr::Event thread_to_ui_event;
	ctr::Event ui_to_thread_event;
	Result last_result;
	ThreadState state;

	thread_data() :
		thread_to_ui_event(ctr::Event::ResetType::Oneshot),
		ui_to_thread_event(ctr::Event::ResetType::Oneshot)
	{}
};

template <typename Downloader>
static void install_generic_thread(thread_data<Downloader> *data)
{
	std::string url;
	Result res;

	data->downloader->set_notify_event(&data->thread_to_ui_event);

	if(!ISET_RESUME_DOWNLOADS)
	{
		res = (*data->get_url)(url);
		if(R_FAILED(res))
		{
			elog("failed to fetch URL: %08lX", res);
			goto out;
		}
		data->downloader->set_target(url, HTTPC_METHOD_GET);
		res = data->downloader->execute_once();
	}
	else
	{
		while(data->state == ThreadState::Installing)
		{
			res = (*data->get_url)(url);
			if(R_SUCCEEDED(res))
			{
				data->downloader->set_target(url, HTTPC_METHOD_GET);
				res = data->downloader->execute_once();
				if(R_FAILED(res))
					elog("Failed to execute_once: %08lX", res);
			}
			else elog("Failed to fetch URL: %08lX", res);

			if(R_MODULE(res) == RM_HTTP)
			{
				/* there has probably been a timeout, we have to
				 * show this to the user and continue in due time */
				data->last_result = res;
				data->state = ThreadState::Timeout;
				data->thread_to_ui_event.signal();
				data->ui_to_thread_event.wait();
				/* the ui thread will set data->state to ThreadState::Insalling
				 * or ThreadState::Abort depending on what the user wants */
				continue;
			}

			/* Two cases:
			 *  - install succeeded: result is 0 and the above if is not reached; we want to break
			 *  - install failed (non-http error): result is not 0 and the above is not reached; we want to break */
			break;
		}
		if(data->state == ThreadState::Abort)
			res = APPERR_CANCELLED;
	}

out:
	data->last_result = res;
	data->state = ThreadState::Finished;
	data->thread_to_ui_event.signal();
	return;
}

template <typename Downloader>
static Result install_generic(Downloader *downloader, get_url_func *get_url, prog_func *on_progress)
{
	thread_data<Downloader> data;
	data.state = ThreadState::Installing;
	data.downloader = downloader;
	data.get_url = get_url;
	data.last_result = 0;

	ctr::thread<thread_data<Downloader> *> thread(install_generic_thread<Downloader>, 1, &data);
	for(;;)
	{
		/* Never block the UI indefinitely on a network backend. This also
		 * keeps B/START cancellation responsive while DNS/connect/recv waits. */
		data.thread_to_ui_event.wait(100 * 1000 * 1000LL);
		/* te installation process is finished; we can exit this loop */
		if(data.state == ThreadState::Finished)
			break;
		/* the install thread wants us to display a timeout to the user */
		else if(data.state == ThreadState::Timeout)
		{
			if(ui::timeoutscreen(data.last_result, 10)) data.state = ThreadState::Abort;
			else                                        data.state = ThreadState::Installing;
			(*on_progress)(downloader->downloaded(), downloader->maybe_total_size());
			data.ui_to_thread_event.signal();
		}
		/* the install thread wants us to simply update the progress */
		else if(data.state == ThreadState::Installing)
			(*on_progress)(downloader->downloaded(), downloader->maybe_total_size());
		/* now we check if the user wants to abort */
		/* TODO: Ideally we'd subscribe to an event that tells us when
		 *       a button is pressed... */
		ui::scan_keys();
		/* The abort() will cause the ResumableDownload to return APPERR_CANCELLED, which in
		 * turn is futher thrown down more to cause an abort when ready to cancel */
		if(!aptMainLoop() || ((ui::kDown() | ui::kHeld()) & (KEY_B | KEY_START)))
			downloader->abort();
	}

	thread.join();
	return data.last_result;
}

static bool have_enough_space(ctr::InstallDestination dest, u32 min_space)
{
	u64 freeSpace;
	Result res;
	if(R_FAILED(res = ctr::mng::get_free_space(dest, &freeSpace)))
		return false;
	return min_space <= freeSpace;
}

void install::global_abort()
{
	if(active_cia_handle != CIA_HANDLE_INVALID)
	{
		AM_CancelCIAInstall(active_cia_handle);
		svcCloseHandle(active_cia_handle);
		active_cia_handle = CIA_HANDLE_INVALID;
	}
}

struct TitleInformation {
	ctr::title_id tid;
	bool isKTR;
	u16 version;
};

/* Network receive and AM writes used to run strictly one after the other.
 * This bounded two-slot queue overlaps them without changing write order or
 * requiring a temporary CIA on the SD card. */
class CiaWritePipeline
{
public:
	CiaWritePipeline(Handle& handle) : ciaHandle(handle)
	{
		LightLock_Init(&this->lock);
		CondVar_Init(&this->hasData);
		CondVar_Init(&this->hasSpace);

		for(size_t i = 0; i < slot_count; ++i)
		{
			this->slots[i].data = (u8 *)memalign(0x1000, http::ResumableDownload::ChunkMaxSize);
			panic_assert(this->slots[i].data, "failed to allocate CIA pipeline buffer");
		}

		s32 priority = 0;
		svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
		/* Core 2 exists only on New 3DS. Old 3DS/2DS keeps the same ordered,
		 * buffered pipeline on the default application core. */
		int writer_core = ctr::mng::is_n3ds() ? 2 : -2;
		this->writer = threadCreate(&CiaWritePipeline::writer_entry, this,
			64 * 1024, priority - 1, writer_core, false);
		/* Be defensive if Core 2 is unavailable despite hardware detection. */
		if(!this->writer && writer_core == 2)
			this->writer = threadCreate(&CiaWritePipeline::writer_entry, this,
				64 * 1024, priority - 1, -2, false);
		panic_assert(this->writer != nullptr, "failed to create CIA writer thread");
	}

	~CiaWritePipeline()
	{
		this->finish();
		for(size_t i = 0; i < slot_count; ++i)
			free(this->slots[i].data);
	}

	Result enqueue(u64 offset, const void *data, size_t size)
	{
		LightLock_Lock(&this->lock);
		while(this->slots[this->produce].full && R_SUCCEEDED(this->writeResult))
			CondVar_Wait(&this->hasSpace, &this->lock);

		if(R_FAILED(this->writeResult))
		{
			Result res = this->writeResult;
			LightLock_Unlock(&this->lock);
			return res;
		}

		Slot& slot = this->slots[this->produce];
		memcpy(slot.data, data, size);
		slot.offset = offset;
		slot.size = size;
		slot.full = true;
		this->produce = (this->produce + 1) % slot_count;
		CondVar_WakeUp(&this->hasData, 1);
		LightLock_Unlock(&this->lock);
		return 0;
	}

	Result finish()
	{
		if(!this->writer)
			return this->writeResult;

		LightLock_Lock(&this->lock);
		this->stopping = true;
		CondVar_WakeUp(&this->hasData, ARBITRATION_SIGNAL_ALL);
		CondVar_WakeUp(&this->hasSpace, ARBITRATION_SIGNAL_ALL);
		LightLock_Unlock(&this->lock);

		threadJoin(this->writer, U64_MAX);
		threadFree(this->writer);
		this->writer = nullptr;
		return this->writeResult;
	}

private:
	static constexpr size_t slot_count = 2;
	struct Slot {
		u8 *data = nullptr;
		u64 offset = 0;
		u32 size = 0;
		bool full = false;
	};

	static void writer_entry(void *arg)
	{
		((CiaWritePipeline *)arg)->writer_loop();
	}

	void writer_loop()
	{
		for(;;)
		{
			LightLock_Lock(&this->lock);
			while(!this->slots[this->consume].full && !this->stopping)
				CondVar_Wait(&this->hasData, &this->lock);

			if(!this->slots[this->consume].full && this->stopping)
			{
				LightLock_Unlock(&this->lock);
				break;
			}

			Slot& slot = this->slots[this->consume];
			u8 *data = slot.data;
			u64 offset = slot.offset;
			u32 size = slot.size;
			LightLock_Unlock(&this->lock);

			u32 written = 0;
			Result res = FSFILE_Write(this->ciaHandle, &written, offset, data, size, 0);

			LightLock_Lock(&this->lock);
			if(R_FAILED(res))
				this->writeResult = res;
			slot.full = false;
			this->consume = (this->consume + 1) % slot_count;
			CondVar_WakeUp(&this->hasSpace, ARBITRATION_SIGNAL_ALL);
			if(R_FAILED(res))
			{
				CondVar_WakeUp(&this->hasData, ARBITRATION_SIGNAL_ALL);
				LightLock_Unlock(&this->lock);
				break;
			}
			LightLock_Unlock(&this->lock);
		}
	}

	Handle& ciaHandle;
	Slot slots[slot_count];
	size_t produce = 0;
	size_t consume = 0;
	bool stopping = false;
	Result writeResult = 0;
	LightLock lock;
	CondVar hasData;
	CondVar hasSpace;
	Thread writer = nullptr;
};

template <typename Downloader>
static Result install_generic_cia_impl(get_url_func *get_url, prog_func *on_progress, bool reinstallable, const TitleInformation& info, bool hsapi_enabled, bool do_ver_check, bool dev_auth)
{
	panic_assert(active_cia_handle == CIA_HANDLE_INVALID, "May only install one CIA at a time");

	/* firstly we perform the prerequisite checks */
	FS_MediaType dest = info.tid.installation_media();
	Result res;

	/* check if the title is already installed and perhaps reinstall */
	bool has_ticket = ctr::mng::ticket_exists(info.tid);
	bool has_title  = ctr::mng::title_exists(info.tid); /* we do not need to check on gamecart here because we can't reinstall on gamecart (duh) */
	if(has_ticket && !has_title)
	{
		ctr::mng::delete_ticket(info.tid.raw);
		/* reload dbs */
		AM_QueryAvailableExternalTitleDatabase(NULL);
	}
	AM_TitleEntry entry;
	/* only reinstall if we want to update */
	/* TODO: We should probably attempt to get this info from the CIA when the first chunk downloads */
	if(has_title && !(R_FAILED(ctr::mng::get_title_entry(info.tid.raw, entry)) || entry.version > info.version))
	{
		if(reinstallable || ISET_DEFAULT_REINSTALL)
		{
			FS_MediaType mydest;
			ctr::title_id mytid;
			if(R_FAILED(res = APT_GetAppletInfo((NS_APPID) envGetAptAppId(), &mytid.raw, (u8 *) &mydest, nullptr, nullptr, nullptr)))
				return res;
			/* we can only delete titles that are not ourselves */
			if(envIsHomebrew() || mytid != info.tid || mydest != dest)
			{
				if(R_FAILED(res = ctr::mng::delete_title(info.tid.raw, ctr::mng::DeleteTitleFlag::DeleteTicket | ctr::mng::DeleteTitleFlag::CheckExist)))
					return res;
				/* reload dbs */
				AM_QueryAvailableExternalTitleDatabase(NULL);
			}
		}
		else return APPERR_NOREINSTALL;
	}

	/* check if the title is meant exclusively for the "new" series */
	if(info.isKTR && !ctr::mng::is_n3ds())
		return APPERR_NOSUPPORT;

	/* we can only start the CIA installation a bit later due to
	 * some checks requiring file size, which is gotten through
	 * downloader.on_total_size_try_get() */

	Downloader downloader(hsapi_enabled, do_ver_check, dev_auth);
	Handle ciaHandle = CIA_HANDLE_INVALID;
	CiaWritePipeline pipeline(ciaHandle);

	downloader.on_total_size_try_get([&]() -> Result {
		if(!downloader.maybe_total_size())
			return APPERR_NOSIZE;
		/* we do a little "do we have the required size" check before we do the actual work */
		ctr::InstallDestination dest = info.tid.detect_dest();
		if(!have_enough_space(dest, downloader.maybe_total_size()))
			return APPERR_NOSPACE;

		/* just here can we actually start installing */
		res = AM_StartCiaInstall(dest.to_mediatype(), &ciaHandle);
		if(R_FAILED(res)) ciaHandle = CIA_HANDLE_INVALID;
		active_cia_handle = ciaHandle;
		return res;
	});

	downloader.on_chunk([&](size_t chunk_size) -> Result {
		return pipeline.enqueue(downloader.downloaded(), downloader.data_buffer(), chunk_size);
	});

	/* TODO: Figure out why pressing HOME actually crashes instead of disallowing it */
	bool homeAllowedRestore = aptIsHomeAllowed();
	aptSetHomeAllowed(false);

	res = install_generic(&downloader, get_url, on_progress);
	Result write_res = pipeline.finish();
	if(R_SUCCEEDED(res) && R_FAILED(write_res))
		res = write_res;
	ilog("install_generic returned %08lX", res);

	/* finalize install */
	if(ciaHandle != CIA_HANDLE_INVALID)
	{
		if(R_FAILED(res)) AM_CancelCIAInstall(ciaHandle);
		else              res = AM_FinishCiaInstall(ciaHandle);
		svcCloseHandle(ciaHandle);
	}
	active_cia_handle = CIA_HANDLE_INVALID;

	aptSetHomeAllowed(homeAllowedRestore);

	ilog("final return of install_generic_cia is %08lX", res);
	return res;
}

static Result install_generic_cia(get_url_func *get_url, prog_func *on_progress, bool reinstallable, const TitleInformation& info, bool hsapi_enabled, bool do_ver_check, bool dev_auth)
{
	return install_generic_cia_impl<http::ResumableDownload>(
		get_url, on_progress, reinstallable, info, hsapi_enabled, do_ver_check, dev_auth);
}

Result install::net_cia(get_url_func get_url, ctr::title_id tid, prog_func prog, bool reinstallable, bool hsapi_enabled, bool do_ver_check, bool dev_auth)
{
	/* net_cia: both hsapi and do_ver_check should be configurable, we may not be downloading from hs */
	return install_generic_cia(&get_url, &prog, reinstallable, { tid, false, 0 }, hsapi_enabled, do_ver_check, dev_auth);
}

Result install::hs_cia(const hsapi::Title& meta, prog_func prog, bool reinstallable)
{
	Result res;
	get_url_func get_url = [meta](std::string& ret) -> Result {
		return hsapi::get_download_link(ret, meta);
	};

	if(meta.flags & hsapi::TitleFlag::installer)
	{
		ilog("installing installer content");

		expandable_binary_data_type content;
		http::ResumableDownload downloader(true, true, true);

		downloader.on_total_size_try_get([&]() -> Result {
			if(!downloader.maybe_total_size())
				return APPERR_NOSIZE;
			/* file forwarders will always install to the SD card */
			if(!have_enough_space(ctr::InstallDestination::SDMC, downloader.maybe_total_size()))
				return APPERR_NOSPACE;
			content.reserve(downloader.maybe_total_size());
			return 0;
		});

		downloader.on_chunk([&](size_t chunk_size) -> Result {
			content.append(downloader.data_buffer(), chunk_size);
			return 0;
		});

		res = install_generic(&downloader, &get_url, &prog);
		if(R_FAILED(res)) return res;
		return install_forwarder((u8 *) content.c_str(), content.size());
	}
	else
	{
		TitleInformation info {
			meta.tid,
			(meta.flags & hsapi::TitleFlag::is_ktr) || strncmp(meta.prod.c_str(), "KTR-", 4) == 0,
			meta.version
		};
		bool use_direct = ISET_DIRECT_CDN_EXPERIMENTAL
			&& ctr::mng::is_n3ds()
			&& !ISET_PROXY_ENABLED;
		if(use_direct)
		{
			ilog("Using experimental direct-socket CDN transport");
			direct_cdn_active = true;
			res = install_generic_cia_impl<DirectCdnDownload>(
				&get_url, &prog, reinstallable, info, true, true, true);
			direct_cdn_active = false;
			if(res == APPERR_DIRECT_SOCKET_SETUP)
			{
				ilog("Direct CDN setup failed; falling back to Nintendo HTTP");
				res = install_generic_cia(&get_url, &prog, reinstallable,
					info, true, true, true);
			}
		}
		else
			res = install_generic_cia(&get_url, &prog, reinstallable,
				info, true, true, true); /* hs_cia: hsapi, do ver check, use dev auth */
	}
	return res;
}

template <typename Downloader>
static Result hs_network_benchmark_impl(hsapi::hid id, install::NetworkBenchmarkResult& result, prog_func prog)
{
	static constexpr u64 benchmark_size = 32ULL * 1024ULL * 1024ULL;
	Downloader downloader(true, true, true);
	u64 target_size = benchmark_size;
	u64 received = 0;
	u64 started = 0;
	u64 previous_time = 0;
	u64 previous_bytes = 0;
	bool reached_target = false;

	downloader.on_total_size_try_get([&]() -> Result {
		if(!downloader.maybe_total_size())
			return APPERR_NOSIZE;
		target_size = std::min<u64>(benchmark_size, downloader.maybe_total_size());
		started = previous_time = osGetTime();
		return 0;
	});

	downloader.on_chunk([&](size_t chunk_size) -> Result {
		received += chunk_size;
		u64 now = osGetTime();
		u64 delta_ms = now - previous_time;
		if(delta_ms)
		{
			float instant = ((received - previous_bytes) / (1024.0f * 1024.0f))
				/ (delta_ms / 1000.0f);
			if(instant > result.peak_mib_s)
				result.peak_mib_s = instant;
		}
		previous_time = now;
		previous_bytes = received;

		if(received >= target_size)
		{
			reached_target = true;
			return APPERR_CANCELLED;
		}
		return 0;
	});

	get_url_func get_url = [id](std::string& ret) -> Result {
		return hsapi::get_download_link(ret, id);
	};

	Result res = install_generic(&downloader, &get_url, &prog);
	if(reached_target && res == APPERR_CANCELLED)
		res = 0;

	result.bytes = received;
	if(started)
		result.elapsed_ms = osGetTime() - started;
	if(result.elapsed_ms)
		result.average_mib_s = (received / (1024.0f * 1024.0f))
			/ (result.elapsed_ms / 1000.0f);
	return res;
}

Result install::hs_network_benchmark(hsapi::hid id, NetworkBenchmarkResult& result, prog_func prog)
{
	bool use_direct = ISET_DIRECT_CDN_EXPERIMENTAL
		&& ctr::mng::is_n3ds()
		&& !ISET_PROXY_ENABLED;
	if(use_direct)
	{
		direct_cdn_active = true;
		Result res = hs_network_benchmark_impl<DirectCdnDownload>(id, result, prog);
		direct_cdn_active = false;
		if(res != APPERR_DIRECT_SOCKET_SETUP)
			return res;
		ilog("Direct CDN benchmark setup failed; using Nintendo HTTP");
		result = NetworkBenchmarkResult {};
	}
	return hs_network_benchmark_impl<http::ResumableDownload>(id, result, prog);
}
