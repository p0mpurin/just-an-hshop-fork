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

#ifndef inc_configuration_h
#define inc_configuration_h

#ifdef __cplusplus
extern "C" {
#endif

struct audio_configuration {
	int default_playlist_options;
	char *default_playlist_name;
	int alwaysMono;
	int nplaylists;
	int palloc;
	struct playlist **playlists;
};

enum audio_config_error {
	ACE_NONE    = 0,
	ACE_FILE_IO = 1,
	ACE_MEM     = 2,
	ACE_PLIST   = 3,
};


struct playlist *acfg_find_playlist(const char *name);
void acfg_add_playlist(struct playlist *pl);
struct audio_configuration *acfg(void);
void acfg_delete_playlist(int pos);
int acfg_realise(void);
void acfg_free(void);
int acfg_load(void);
int acfg_save(void);

#ifdef __cplusplus
}
#endif

#endif

