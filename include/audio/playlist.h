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

#ifndef inc_playlist_h
#define inc_playlist_h

#ifdef __cplusplus
extern "C" {
#endif

enum playlist_flags {
	SP_NONE      = 0,
	SP_RANDOMISE = 1,
	SP_REPEAT    = 2,
};

struct playlist_item {
	struct playlist_item *next;
	struct playlist_item *prev;
	char filename[];
};

struct playlist {
	struct playlist_item *head;
	struct playlist_item *tail;
	int size;
	char name[];
};

int plist_1sized_list(void);
struct playlist_item *plist_prev(void);
struct playlist_item *plist_next(void);
void plist_init(void);
void plist_exit(void);
struct playlist *plist_current(void);
static inline void plist_set_flags(int nf) { extern int playlist_selection_params; playlist_selection_params = nf; }
static inline int plist_get_flags() { extern int playlist_selection_params; return playlist_selection_params; }
void plist_unselect(void);

int playlist_append(struct playlist *pl, const char *prefix, const char *filename);
void playlist_unlink_item(struct playlist *pl, struct playlist_item *pi);
void playlist_swap(struct playlist *pl, int i1, int i2);
struct playlist *playlist_make(const char *name);
void playlist_free(struct playlist *pl);
int playlist_use(struct playlist *pl);

#ifdef __cplusplus
}
#endif

#endif

