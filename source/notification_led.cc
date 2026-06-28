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

#include "notification_led.hh"

#include <ui/base.hh>

#include <3ds.h>
#include <algorithm>
#include <string.h>

#include "log.hh"
#include "settings.hh"

namespace
{
	bool g_led_failed = false;
	bool g_installing = false;
	int g_last_progress_step = -1;

	bool led_try(ui::LED::Pattern *pattern)
	{
		if(!ISET_ALLOW_LED)
			return false;
		if(g_led_failed)
			return false;

		Result res = ui::LED::SetPattern(pattern);
		if(R_FAILED(res))
		{
			g_led_failed = true;
			elog("notification LED disabled after SetPattern failed: %08lX", res);
			return false;
		}

		return true;
	}

	bool solid(u8 r, u8 g, u8 b)
	{
		ui::LED::Pattern pattern;
		ui::LED::Solid(&pattern, UI_LED_MAKE_ANIMATION(0, 0xFF, 0), r, g, b);
		return led_try(&pattern);
	}

	void blink(u8 r, u8 g, u8 b)
	{
		ui::LED::Pattern pattern;
		memset(&pattern, 0, sizeof(pattern));
		pattern.animation = UI_LED_MAKE_ANIMATION(0x20, 0x00, 0xFF);

		for(size_t i = 0; i < 32; ++i)
		{
			bool on = (i / 4) % 2 == 0;
			pattern.red_pattern[i] = on ? r : 0;
			pattern.green_pattern[i] = on ? g : 0;
			pattern.blue_pattern[i] = on ? b : 0;
		}

		if(led_try(&pattern))
			svcSleepThread(1200 * 1000 * 1000LL);
		Led_Off();
	}
}

void Led_Init()
{
	g_led_failed = false;
	g_installing = false;
	g_last_progress_step = -1;
	Led_Off();
}

void Led_SetProgress(int percent)
{
	percent = std::max(0, std::min(100, percent));
	int step = percent / 5;
	if(step == g_last_progress_step)
		return;

	g_installing = false;
	g_last_progress_step = step;
	u8 green = (u8)((percent * 255) / 100);
	u8 blue = (u8)(255 - green);
	solid(0, green, blue);
}

void Led_SetDownloadingProgress(int percent)
{
	Led_SetProgress(percent);
}

void Led_SetInstalling()
{
	if(g_installing)
		return;
	g_installing = true;
	g_last_progress_step = -1;
	solid(0, 0xFF, 0);
}

void Led_SetDone()
{
	g_last_progress_step = -1;
	g_installing = false;
	blink(0, 0xFF, 0);
}

void Led_SetError()
{
	g_last_progress_step = -1;
	g_installing = false;
	blink(0xFF, 0, 0);
}

void Led_SetCancelled()
{
	g_last_progress_step = -1;
	g_installing = false;
	if(solid(0x80, 0, 0xFF))
		svcSleepThread(500 * 1000 * 1000LL);
	Led_Off();
}

void Led_Off(bool force)
{
	if((!force && !ISET_ALLOW_LED) || g_led_failed)
		return;
	Result res = ui::LED::ResetPattern(force);
	if(R_FAILED(res))
	{
		g_led_failed = true;
		elog("notification LED disabled after ResetPattern failed: %08lX", res);
	}
}
