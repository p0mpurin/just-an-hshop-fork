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
#include <ctr/dmn.hh>

static u32 sleep_lock_refcount = 0;
enum {
	NoLock         = 0,
	SleepAllowed   = 1,
	ExclusiveState = 2,
	LockState      = 4,
};
int sleep_lock_state = NoLock;

Result ctr::dmn::increase_sleep_lock_ref()
{
	if(!sleep_lock_refcount++)
	{
		/* basically ensures that we can use the network during sleep
		 * thanks Kartik for the help */
		aptSetSleepAllowed(false);
		sleep_lock_state |= SleepAllowed;
		Result res;
		if(R_FAILED(res = NDMU_EnterExclusiveState(NDM_EXCLUSIVE_STATE_INFRASTRUCTURE)))
			return res;
		sleep_lock_state |= ExclusiveState;
		if(R_FAILED(res = NDMU_LockState()))
			return res;
		sleep_lock_state |= LockState;
	}
	return 0;
}

void ctr::dmn::decrease_sleep_lock_ref()
{
	if(sleep_lock_refcount && !--sleep_lock_refcount)
	{
		if(sleep_lock_state & LockState) NDMU_UnlockState();
		if(sleep_lock_state & ExclusiveState) NDMU_LeaveExclusiveState();
		if(sleep_lock_state & SleepAllowed) aptSetSleepAllowed(true);
		sleep_lock_state = NoLock;
	}
}

void ctr::dmn::delete_sleep_lock()
{
	if(sleep_lock_refcount)
	{
		sleep_lock_refcount = 1;
		ctr::dmn::decrease_sleep_lock_ref();
	}
}