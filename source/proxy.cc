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

#include "proxy.hh"

#include <stdio.h>

#include "settings.hh"
#include "panic.hh"
#include "i18n.hh"

#define PROXYFILE "/3ds/Rune3DS/proxy"

// https://3dbrew.org/wiki/HTTPC:SetProxy
static Result httpcSetProxy(httpcContext *context, u16 port, u32 proxylen, const char *proxy,
	u32 usernamelen, const char *username, u32 passwordlen, const char *password)
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0]  = IPC_MakeHeader(0x000D, 0x5, 0x6); // 0x000D0146
	cmdbuf[1]  = context->httphandle;
	cmdbuf[2]  = proxylen;
	cmdbuf[3]  = port & 0xFFFF;
	cmdbuf[4]  = usernamelen;
	cmdbuf[5]  = passwordlen;
	cmdbuf[6]  = (proxylen << 14) | 0x2;
	cmdbuf[7]  = (u32) proxy;
	cmdbuf[8]  = (usernamelen << 14) | 0x402;
	cmdbuf[9]  = (u32) username;
	cmdbuf[10] = (passwordlen << 14) | 0x802;
	cmdbuf[11] = (u32) password;

	Result ret = 0;
	if(R_FAILED(ret = svcSendSyncRequest(context->servhandle)))
		return ret;

	return cmdbuf[1];
}

static Result httpcSetProxyDefault(httpcContext *context)
{
	u32 *cmdbuf = getThreadCommandBuffer();
	
	cmdbuf[0] = IPC_MakeHeader(0x000E, 1, 0);
	cmdbuf[1] = context->httphandle;
	
	Result ret = 0;
	if(R_FAILED(ret = svcSendSyncRequest(context->servhandle)))
		return ret;

	return cmdbuf[1];
}

static inline Result httpcSetProxy(httpcContext *context, u16 port,
	const std::string& proxy, const std::string& username, const std::string& password)
{
	return httpcSetProxy(
		context, port, proxy.size() + 1, proxy.c_str(),
		username.size() + 1, username.size() ? username.c_str() : "",
		password.size() + 1, password.size() ? password.c_str() : ""
	);
}

bool proxy::check()
{
	if (!ISET_PROXY_ENABLED)
		return false;
	
	NewSettings *set = get_nsettings();
	
	if (!set->proxy_port || set->proxy_port > 65535)
		return false;
	
	if (!set->proxy_host.size())
		return false;
	
	if (!set->proxy_username.size() && set->proxy_password.size())
		return false;
	
	return true;
}

Result proxy::apply(httpcContext *context)
{
	NewSettings *ns = get_nsettings();
	if (proxy::check()) {
		return httpcSetProxy(context, ns->proxy_port, ns->proxy_host,
							ns->proxy_username, ns->proxy_password);
	}

	// Not set means use system proxy, if present
	return httpcSetProxyDefault(context);
}

static bool is_CRLF(const std::string& buf, std::string::size_type i)
{
	return buf[i == 0 ? 0 : i - 1] == '\r';
}

static void put_colonsep(const std::string& buf, std::string& p1, std::string& p2)
{
	std::string::size_type colon = buf.find(":");
	if(colon == std::string::npos)
		panic(STRING(invalid_proxy));

	p1 = buf.substr(0, colon);
	// +1 to remove colon
	p2 = buf.substr(colon + 1);
}

static bool validate(const proxy::legacy::Params& p)
{
	/* no proxy set is always valid */
	if(p.host == "")
		return true;

	// 0xFFFF = overflow, 0xFFFF+ are invalid ports
	if(p.port == 0 || p.port >= 0xFFFF)
		return false;

	return true;
}


void proxy::legacy::parse(proxy::legacy::Params& p)
{
	FILE *proxyfile = fopen(PROXYFILE, "r");
	if(proxyfile == NULL) return;

	fseek(proxyfile, 0, SEEK_END);
	size_t fsize = ftell(proxyfile);
	fseek(proxyfile, 0, SEEK_SET);

	if(fsize == 0)
	{
		fclose(proxyfile);
		return;
	}

	char *buf = new char[fsize];
	if(fread(buf, 1, fsize, proxyfile) != fsize)
		panic(STRING(invalid_proxy));
	std::string strbuf(buf, fsize);

	fclose(proxyfile);
	delete [] buf;

	std::string::size_type line = strbuf.find("\n");
	if(line == std::string::npos)
		line = strbuf.size() - 1;

	bool crlf = is_CRLF(strbuf, line);
	std::string proxyport = strbuf.substr(0, crlf ? line - 1 : line);

	if(line != strbuf.size() - 1)
	{
		std::string::size_type lineuser = strbuf.find("\n", line);
		if(line == std::string::npos)
			line = strbuf.size() - 1;
		crlf = is_CRLF(strbuf, lineuser);

		// Skip \n
		std::string userpasswd = strbuf.substr(line + 1, crlf ? line - 1 : line);
		if(userpasswd != "")
			put_colonsep(userpasswd, p.username, p.password);
	}

	std::string port;
	put_colonsep(proxyport, p.host, port);

	p.port = strtoul(port.c_str(), nullptr, 10);
	if(!validate(p)) panic(STRING(invalid_proxy));
}

void proxy::legacy::del()
{
	remove(PROXYFILE);
}
