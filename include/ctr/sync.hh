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
#ifndef _inc_ctr_sync_hh
#define _inc_ctr_sync_hh

#include <3ds.h>

namespace ctr
{
	class Event
	{
	public:
		enum class ResetType {
			Oneshot = RESET_ONESHOT,
			Sticky  = RESET_STICKY,
		};

		Event(ResetType type) { LightEvent_Init(&this->event, (::ResetType) type); }
		LightEvent *get_event() { return &this->event; }
		void clear() { LightEvent_Clear(&this->event); }
		void pulse() { LightEvent_Pulse(&this->event); }
		void signal() { LightEvent_Signal(&this->event); }

		int wait(s64 timeout_ns) { return LightEvent_WaitTimeout(&this->event, timeout_ns); }
		int try_wait() { return LightEvent_TryWait(&this->event); }
		void wait() { LightEvent_Wait(&this->event); }

	private:
		LightEvent event;

	};

	class Lock
	{
	public:
		Lock() { LightLock_Init(&this->llock); }
		LightLock *get_lock() { return &this->llock; }
		void lock() { LightLock_Lock(&this->llock); }
		void unlock() { LightLock_Unlock(&this->llock); }
	private:
		LightLock llock;
	};

	class LockedInScope
	{
	public:
		LockedInScope(LightLock *lock) { LightLock_Lock(this->lock = lock); }
		LockedInScope(Lock& lock) { LightLock_Lock(this->lock = lock.get_lock()); }
		~LockedInScope() { LightLock_Unlock(this->lock); }
	private:
		LightLock *lock;
	};

	class Semaphore
	{
	public:
		Semaphore(s16 initial = 1, s16 max = 1) { LightSemaphore_Init(&this->lsemaphore, initial, max); }

		void Acquire(s32 count = 1) { LightSemaphore_Acquire(&this->lsemaphore, count); }
		bool TryAcquire(s32 count = 1) { return LightSemaphore_TryAcquire(&this->lsemaphore, count) == 0; }

		void Release(s32 count = 1) { LightSemaphore_Release(&this->lsemaphore, count); }

	private:
		LightSemaphore lsemaphore;
	};
}

#endif