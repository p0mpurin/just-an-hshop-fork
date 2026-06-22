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

#include "audio/playlist.h"
#include "audio/player.h"
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "log.hh"

/* playlists are represented as a linked list of filenames
 *  there is a list of playlist heads which maps them to a key of the playlist name
 */

static struct playlist_state {
	int pos;
	struct playlist *selected_list;
	struct playlist_item *items[];
} *pstate = NULL;

int playlist_selection_params = SP_NONE;


static int rand_index(int size)
{
	/* this is not correctly distributed but whatever */
	return rand() % size;
}

static void plist_rearrange()
{
	pstate->pos = 0;
	if(playlist_selection_params & SP_RANDOMISE)
	{
		memset(pstate->items, 0, sizeof(struct playlist_item *) * pstate->selected_list->size);
		int i;
		for(struct playlist_item *cur = pstate->selected_list->head; cur; cur = cur->next)
		{
			do
				i = rand_index(pstate->selected_list->size);
			while(pstate->items[i]);
			pstate->items[i] = cur;
		}
	}
	else
	{
		struct playlist_item *cur = pstate->selected_list->head;
		for(int i = 0; cur; ++i, cur = cur->next)
			pstate->items[i] = cur;
	}
}

static int plist_set_state_for(struct playlist *pl)
{
	if(pstate) free(pstate);
	pstate = malloc(sizeof(struct playlist_state) + sizeof(struct playlist_item *) * pl->size);
	if(!pstate) return 0;
	pstate->selected_list = pl;
	plist_rearrange();
	return 1;
}

/* global */

void plist_unselect(void)
{
	if(pstate)
		free(pstate);
}

struct playlist_item *plist_prev(void)
{
	if(!pstate) return NULL;
	if(pstate->selected_list->size == 0)
		return NULL;

	if(pstate->pos <= 1)
	{
		/* if we want to repeat we'll wrap around */
		if(playlist_selection_params & SP_REPEAT)
			pstate->pos = pstate->selected_list->size;
		/* otherwise we'll just replay the first track */
		else
			pstate->pos = 1;
	}
	else --pstate->pos;

	return pstate->items[pstate->pos - 1];
}

int plist_1sized_list(void)
{
	if(!pstate) return 0;
	return pstate->selected_list->size == 1;
}

struct playlist_item *plist_next(void)
{
	if(!pstate) return NULL;
	if(pstate->selected_list->size == 0)
		return NULL;

	if(pstate->pos == pstate->selected_list->size)
	{
		/* finished */
		if(!(playlist_selection_params & SP_REPEAT))
		{
			free(pstate);
			pstate = NULL;
			return NULL;
		}
		plist_rearrange();
	}

	return pstate->items[pstate->pos++];
}

void plist_init(void)
{
	srand(time(NULL));
}

void plist_exit(void)
{
	if(pstate)
		free(pstate);
}

/* playlist */

struct playlist *playlist_make(const char *name)
{
	size_t slen = strlen(name) + 1;
	struct playlist *ret = malloc(sizeof(struct playlist) + slen);
	if(!ret) return NULL;
	ret->head = ret->tail = NULL;
	ret->size = 0;
	memcpy(ret->name, name, slen);
	return ret;
}

int playlist_append(struct playlist *pl, const char *prefix, const char *filename)
{
	size_t plen = strlen(prefix), flen = strlen(filename);
	struct playlist_item *item = malloc(sizeof(struct playlist_item) + plen + flen + 1);
	if(!item) return 0;
	item->prev = pl->tail;
	item->next = NULL;
	memcpy(item->filename, prefix, plen);
	memcpy(item->filename + plen, filename, flen);
	item->filename[plen + flen] = '\0';
	/* test if the file exists and is readable before inserting */
	if(access(item->filename, R_OK) != 0)
	{
		free(item);
		return 1;
	}
	/* check for duplicates */
	for(struct playlist_item *it = pl->head; it; it = it->next)
		if(strcmp(it->filename, item->filename) == 0)
		{
			free(item);
			return 1;
		}
	if(!pl->head) pl->head = item;
	if(pl->tail)  pl->tail->next = item;
	pl->tail = item;
	++pl->size;
	return 1;
}

void playlist_unlink_item(struct playlist *pl, struct playlist_item *pi)
{
	player_continue_if_playing(pi);
	player_wait_commands();
	if(pi->next) pi->next->prev = pi->prev;
	if(pi->prev) pi->prev->next = pi->next;
	if(pl->head == pi) pl->head = pi->next;
	if(pl->tail == pi) pl->tail = pi->prev;
	--pl->size;
	free(pi);
}

int playlist_use(struct playlist *pl)
{
	if(!pl) return 1;
	ilog("using playlist: %s", pl->name);
	if(!plist_set_state_for(pl))
	{
		elog("failed to set playlist state; audio will fail to work");
		return 0;
	}
	/* update the player to refersh */
	player_refresh_playlist();
	return 1;
}

struct playlist *plist_current(void)
{
	return pstate ? pstate->selected_list : NULL;
}

void playlist_free(struct playlist *pl)
{
	struct playlist_item *item, *next = pl->head;
	while((item = next))
	{
		next = item->next;
		free(item);
	}
	free(pl);
}

void playlist_swap(struct playlist *pl, int i1, int i2)
{
	if(i1 == i2) return;

	struct playlist_item *first = NULL, *second = NULL, *it = pl->head, *tmpA;
	for(int i = 0; it; ++i, it = it->next)
		if(i == i1 || i == i2)
		{
			if(!first) first = it;
			else
			{
				second = it;
				break;
			}
		}
	if(!first || !second)
		return /* EINVAL */;

	tmpA = first->next;
	first->next = second->next;
	second->next = tmpA;
	/* they were adjecent */
	if(second->next == second)
		second->next = first;

	tmpA = first->prev;
	first->prev = second->prev;
	second->prev = tmpA;
	/* they were adjecent */
	if(first->prev == first)
		first->prev = second;

	/* first always has a prev, since it is the pointer of the latter index */
	first->prev->next = first;
	if(second->prev) second->prev->next = second;
	/* first used to be head, now second will be */
	else pl->head = second;

	/* second always has a next, since it is the pointer of the former index */
	second->next->prev = second;
	if(first->prev) first->prev->next = first;
	/* second used to be tail, now first will be */
	else pl->tail = first;
}

