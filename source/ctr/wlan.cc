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

#include <ctr/wlan.hh>

static Handle g_connect_mtx = 0;

#define WLAN_OP_MAX_TIMEOUT 10000000000LLU

#define WLAN_OP_LOCK \
	if (R_FAILED((res = svcWaitSynchronization(g_connect_mtx, WLAN_OP_MAX_TIMEOUT)))) \
		return res;

#define WLAN_OP_UNLOCK \
	if (R_FAILED((res = svcReleaseMutex(g_connect_mtx)))) \
		return res;

Handle* ctr::wlan::connect_mtx() { return &g_connect_mtx; }

static Result ACU_CloseAsync(Handle closeEvent)
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(8, 0, 4);
	cmdbuf[1] = IPC_Desc_CurProcessId();
	cmdbuf[2] = 0;
	cmdbuf[3] = IPC_Desc_SharedHandles(1);
	cmdbuf[4] = closeEvent;

	Result res = svcSendSyncRequest(*acGetSessionHandle());

	if (R_FAILED(res)) return res;

	return (Result)cmdbuf[1];
}

static Result ACU_GetConnectResult()
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(5, 0, 2); // 0x00050002
	cmdbuf[1] = IPC_Desc_CurProcessId();

	Result res = svcSendSyncRequest(*acGetSessionHandle());

	if (R_FAILED(res)) return res;

	return (Result)cmdbuf[1];
}

static Result ACU_GetCloseResult()
{
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(9, 0, 2); // 0x00050002
	cmdbuf[1] = IPC_Desc_CurProcessId();

	Result res = svcSendSyncRequest(*acGetSessionHandle());

	if (R_FAILED(res)) return res;

	return (Result)cmdbuf[1];
}

static Result ACU_IsConnected(u32 pid, u8 *connected)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x3E, 1, 2); // 0x003E0042
	cmdbuf[1] = pid;
	cmdbuf[2] = IPC_Desc_CurProcessId();
	cmdbuf[3] = 0;

	if (R_FAILED(ret = svcSendSyncRequest(*acGetSessionHandle())))
		return ret;

	*connected = (u8)cmdbuf[2];

	return (Result)cmdbuf[1];
}

static Result ACU_RegisterDisconnectEvent(Handle event)
{
	Result ret = 0;
	u32 *cmdbuf = getThreadCommandBuffer();

	cmdbuf[0] = IPC_MakeHeader(0x30,0,4); // 0x00300004
	cmdbuf[1] = IPC_Desc_CurProcessId();
	// cmdbuf[2] = process id
	cmdbuf[3] = IPC_Desc_SharedHandles(1);
	cmdbuf[4] = event;

	if (R_FAILED(ret = svcSendSyncRequest(*acGetSessionHandle())))
		return ret;

	return (Result)cmdbuf[1];
}

Result ctr::wlan::connect(Handle disconnect_event, u64 timeout_ns)
{
	acuConfig ac_config;
	Handle connect_event = 0;
	Result res = 0;

	WLAN_OP_LOCK

	if(R_FAILED(res = svcCreateEvent(&connect_event, RESET_ONESHOT)))
		return res;

	if(R_FAILED(res = ACU_CreateDefaultConfig(&ac_config)))
		return res;

	if(R_FAILED(res = ACU_SetRequestEulaVersion(&ac_config)))
		return res;

	if(R_FAILED(res = ACU_RegisterDisconnectEvent(disconnect_event)))
		return res;

	if(R_FAILED(res = ACU_ConnectAsync(&ac_config, connect_event)))
		return res;

	if(R_FAILED(res = svcWaitSynchronization(connect_event, timeout_ns)) || R_DESCRIPTION(res) == RD_TIMEOUT)
	{
		svcCloseHandle(connect_event);
		return res;
	}

	svcCloseHandle(connect_event);

	WLAN_OP_UNLOCK

	return ACU_GetConnectResult();
}

Result ctr::wlan::disconnect()
{
	Handle disconnect_event = 0;
	Result res = 0;

	WLAN_OP_LOCK

	if(R_FAILED(res = svcCreateEvent(&disconnect_event, RESET_ONESHOT)))
		return res;

	if(R_FAILED(res = ACU_CloseAsync(disconnect_event)) || (R_FAILED(res = svcWaitSynchronization(disconnect_event, WLAN_OP_MAX_TIMEOUT)) || R_DESCRIPTION(res) == RD_TIMEOUT))
	{
		svcCloseHandle(disconnect_event);
		return res;
	}

	svcCloseHandle(disconnect_event);

	WLAN_OP_UNLOCK

	return ACU_GetCloseResult();
}

u8 ctr::wlan::strength()
{
	return osGetWifiStrength();
}

bool ctr::wlan::is_connected()
{
	Result res = 0;
	/* until libctru fixes it */
	union {
		u16 as_16;
		u32 as_32;
	} acStatus;
	acStatus.as_32 = 0;
	u8 is_connected = 0;

#ifndef EMULATOR
	if (R_FAILED(res = ACU_GetStatus(&acStatus.as_32)) || acStatus.as_16 < 1)
		return false;

	if (R_FAILED(res = ACU_IsConnected(0, &is_connected)) || !is_connected)
		return false;

	return true;
#else
	return true;
#endif
}

bool ctr::wlan::is_enabled()
{
	return OS_SharedConfig->network_state != 7;
}

Result ctr::wlan::enable()
{
	Result res = 0;

	WLAN_OP_LOCK

	res = NWMEXT_ControlWirelessEnabled(true);

	WLAN_OP_UNLOCK

	return res;
}

Result ctr::wlan::disable()
{
	Result res = 0;

	WLAN_OP_LOCK

	res = NWMEXT_ControlWirelessEnabled(false);

	WLAN_OP_UNLOCK

	return res;
}