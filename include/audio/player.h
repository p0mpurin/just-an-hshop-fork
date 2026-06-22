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

#ifndef inc_player_h
#define inc_player_h

#ifdef __cplusplus
extern "C" {
#endif

#include <3ds/types.h>

struct cwav;
struct playlist_item;
typedef void (*player_switch_callback)(const struct cwav *ncw);

void player_set_switch_callback(player_switch_callback cb);
void player_continue_if_playing(struct playlist_item *pi);
void player_play(const char *filename);
void player_refresh_playlist(void);
void player_reconfigure_mix(void);
void player_wait_commands(void);
bool player_is_paused(void);
void player_previous(void);
void player_unpause(void);
Result player_init(void);
void player_pause(void);
void player_next(void);
void player_exit(void);
void player_halt(void);

static inline void player_toggle_pause(void)
{
	player_is_paused() ? player_unpause() : player_pause();
}

#ifdef __cplusplus
}
#endif

#endif

