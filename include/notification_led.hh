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

#ifndef inc_notification_led_hh
#define inc_notification_led_hh

void Led_Init();
void Led_SetProgress(int percent);
void Led_SetDownloadingProgress(int percent);
void Led_SetInstalling();
void Led_SetDone();
void Led_SetError();
void Led_SetCancelled();
void Led_Off(bool force = false);

#endif
