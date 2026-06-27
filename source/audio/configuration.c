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

#include "audio/configuration.h"
#include "audio/playlist.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "log.hh"

#define ACONFIG_MUSIC_DIR "/3ds/Rune3DS/music/"
#define ACONFIG_FILE "/3ds/Rune3DS/audio.cfg"


static const char default_acfg[] =
	"\n"
	"## This is the default Rune3DS audio configuration file\n"
	"## Everything following a pound (#) is ignored as a comment\n"
	"## You may specify options relating to the audio system here\n"
	"## Note that when you click on \"Save\" in the graphical manager all formatting will be discarded\n"
	"\n"
	"## A comma-seperated list of default playlist options, currently they may be:\n"
	"##   none       => no effect\n"
	"##   randomise  => randomise the order of songs in each playlist upon loading\n"
	"##   repeat     => repeat songs/playlists instead of remaining silent upon completion\n"
	"# playlist_options = none\n"
	"\n"
	"## Option to play every file as monostereo audio, even if it has multiple channels\n"
	"## Files sound significantly worse with this option\n"
	"# mono = no\n"
	"\n"
	"## Sets the name of the playlist to initially play when starting up Rune3DS\n"
	"## If this is not set, no audio is played when starting Rune3DS\n"
	"# default_playlist = playlist_name\n"
	"\n"
	"## You may have 0 or more of the following playlists, a list of songs associated with a name.\n"
	"## Note that playlist names may not contain spaces, if you wish for Rune3DS to display them anyway\n"
	"## replace the space with either a \"-\" or a \"_\" and Rune3DS will replace it with a space when displaying.\n"
	"# playlist_name [\n"
	"#   song_name_a.hwav        # path relative to /3ds/Rune3DS/music\n"
	"#   /music/song_name_b.hwav # path relative to the root of the SD card\n"
	"# ]\n"
	"\n";

static struct audio_configuration gacfg;
struct audio_configuration *acfg(void)
{
	return &gacfg;
}

static void acfg_write_file(const char *contents)
{
	FILE *f = fopen(ACONFIG_FILE, "w");
	if(!f) return;
	fwrite(contents, strlen(contents), 1, f);
	fclose(f);
}

static char *acfg_read_file(void)
{
	FILE *f = fopen(ACONFIG_FILE, "r");
	if(!f) return NULL;
	fseek(f, 0, SEEK_END);
	size_t len = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *ret = (char *)malloc(len + 1);
	if(!ret) goto fail;
	ret[len] = '\0';
	if(fread(ret, len, 1, f) != 1)
		goto fail;

out:
	fclose(f);
	return ret;
fail:
	free(ret);
	ret = NULL;
	goto out;
}

static int is_one_of(char ch, const char *chs)
{
	for(const char *i = chs; *i; ++i)
		if(ch == *i)
			return 1;
	return 0;
}

static char *find_one_of(char *ptr, const char *chs)
{
	if(!ptr) return NULL;
	for(; *ptr; ++ptr)
	{
		if(is_one_of(*ptr, chs))
			return ptr;
	}
	return NULL;
}

static char *walk_nonspace_back_until(char *ptr, const char *lim)
{
	while(is_one_of(*(ptr - 1), " \t") && ptr != lim)
		--ptr;
	return ptr;
}

static void prepare_id(char *start, char *end)
{
	*end = '\0';
	for(; *start; ++start)
		*start = tolower(*start);
}

static void skip_ws(char **s)
{
	if(*s)
		while(is_one_of(**s, " \t"))
			++(*s);
}

static char *end_of(char *s)
{
	return s + strlen(s);
}

void acfg_add_playlist(struct playlist *pl)
{
	if(gacfg.nplaylists == gacfg.palloc)
	{
		int npa = gacfg.palloc + 5;
		struct playlist **npls = (struct playlist **)realloc(gacfg.playlists, gacfg.palloc * sizeof(struct playlist *));
		if(!npls)
		{
			playlist_free(pl);
			return;
		}
		gacfg.playlists = npls;
		gacfg.palloc = npa;
	}
	gacfg.playlists[gacfg.nplaylists++] = pl;
}

/* TODO: Clean this code up */
int acfg_load(void)
{
	gacfg.default_playlist_options = SP_NONE;
	gacfg.default_playlist_name = NULL;
	gacfg.nplaylists = 0;
	gacfg.palloc = 5;
	gacfg.alwaysMono = 0;
	gacfg.playlists = (struct playlist **)malloc(sizeof(struct playlist *) * 5);
	if(!gacfg.playlists)
	{
		elog("failed to allocate playlist");
		return ACE_MEM;
	}

	/* no configuration; we use the default one */
	if(access(ACONFIG_FILE, F_OK) != 0)
	{
default_settings:
		acfg_write_file(default_acfg);
		return ACE_NONE;
	}

	char *file = acfg_read_file(), *start, *endid, *vname, *ptr, *key, *next;
	int len;
	struct playlist *pl = NULL;
	if(!file)
	{
		elog("failed to read configuration file, using default settings");
		goto default_settings;
	}
	for(ptr = file; *ptr; ++ptr)
	{
		if(pl)
		{
			/* skip the whitespace */
			while(is_one_of(*ptr, " \t"))
				++ptr;
			endid = find_one_of(ptr, "#\n");
			if(!endid) break; /* eof */
			if(endid == ptr)
				goto next;
			endid = walk_nonspace_back_until(endid, ptr);
			if(*ptr == ']')
			{
				/* the playlist was closed */
				acfg_add_playlist(pl);
				pl = NULL;
			}
			else if(*ptr)
			{
				/* we found a file to add */
				char restore = *endid;
				*endid = '\0';
				if(ptr != endid) /* length > 0 */
					if(!playlist_append(pl, *ptr == '/' ? "" : ACONFIG_MUSIC_DIR, ptr)) /* failed to append */
					{
						/* give up on this playlist and end */
						playlist_free(pl);
						pl = NULL;
						elog("failed to add (%s) to playlist (%s)", ptr, pl->name);
						return ACE_PLIST;
					}
				*endid = restore;
			}

next:
			endid = find_one_of(endid, "\n");
			if(!(ptr = endid))
				break; /* eof */
		}
		else
		{
			if(is_one_of(*ptr, " \t\n"))
				continue; /* whitespace should just be skipped */
			else if(*ptr == '#') /* the entire line should be skipped */
			{
				ptr = find_one_of(ptr, "\n");
				/* finished */
				if(!ptr) break;
			}
			else
			{
				vname = start = ptr;
				endid = find_one_of(start, " \t=[#\n");
				/* invalid */
				if(!endid) continue;
again:
				switch(*endid)
				{
				case '#':
				case '\n': /* invalid */
					break;
				case '\t': /* whitespace: skip and retry */
				case ' ':
					*endid = '\0'; ++endid;
					skip_ws(&endid);
					goto again;
				case '[': /* playlist */
					prepare_id(vname, endid);
					pl = playlist_make(vname);
					break;
				case '=': /* key */
					prepare_id(vname, endid);
					key = endid + 1;
					skip_ws(&key);
					endid = find_one_of(key, "#\n");
					if(!endid) endid = end_of(key);
					endid = walk_nonspace_back_until(endid, key);
					char recover = *endid;
					*endid = '\0';
#define SSTREQ(ss) ((size_t)(key - vname) > sizeof(ss)-1 && strncmp(vname,ss,sizeof(ss)-1)==0)
					if(SSTREQ("playlist_options"))
					{
						gacfg.default_playlist_options = SP_NONE;
						next = key;
						do {
							if(*next == ',')
								++next;
							skip_ws(&next);
							key = next;
							next = find_one_of(next, ",");
							len = next ? (size_t) (next - key) : strlen(key);
							if(strncasecmp(key, "none", len) == 0)
								;
							else if(strncasecmp(key, "randomise", len) == 0 || strncasecmp(key, "randomize", len) == 0)
								gacfg.default_playlist_options |= SP_RANDOMISE;
							else if(strncasecmp(key, "repeat", len) == 0)
								gacfg.default_playlist_options |= SP_REPEAT;
							/* else unknown option */
						} while(next);
					}
					else if(SSTREQ("default_playlist"))
					{
						free(gacfg.default_playlist_name);
						if(*key && strcasecmp(key, "null") != 0) gacfg.default_playlist_name = strdup(key);
						else                                     gacfg.default_playlist_name = NULL;
					}
					else if(SSTREQ("mono"))
					{
						if(strcasecmp(key, "yes") == 0 || strcasecmp(key, "on") == 0)
							gacfg.alwaysMono = 1;
						else
							gacfg.alwaysMono = 0;
					}
#undef SSTREQ
					/* else unknown key */
					*endid = recover;
					break;
				}
				ptr = find_one_of(endid + 1, "\n");
				if(!ptr) break;
				ptr = endid;
			}
		}
	}
	/* non-closed playlist will be appended */
	if(pl) acfg_add_playlist(pl);
	free(file);
	return ACE_NONE;
}

void acfg_delete_playlist(int pos)
{
	dlog("deleting playlist @ %i, nplaylists=%i", pos, gacfg.nplaylists);
	if(plist_current() == gacfg.playlists[pos])
		plist_unselect();
	playlist_free(gacfg.playlists[pos]);
	--gacfg.nplaylists;
	dlog("moving into %i from %i, %i items", pos, pos + 1, gacfg.nplaylists - pos);
	memmove(&gacfg.playlists[pos], &gacfg.playlists[pos + 1], (gacfg.nplaylists - pos) * sizeof(struct playlist *));
}

struct playlist *acfg_find_playlist(const char *name)
{
	if(name)
		for(int i = 0; i < gacfg.nplaylists; ++i)
			if(strcmp(gacfg.playlists[i]->name, gacfg.default_playlist_name) == 0)
				return gacfg.playlists[i];
	return NULL;
}

int acfg_realise(void)
{
	plist_init();
	plist_set_flags(gacfg.default_playlist_options);
	struct playlist *pl = acfg_find_playlist(gacfg.default_playlist_name);
	if(pl) if(!playlist_use(pl))
		return ACE_PLIST;
#ifdef FULL_LOG
	vlog("Full playlist configuration dump:");
	for(int i = 0; i < gacfg.nplaylists; ++i)
	{
		vlog(" %s:", gacfg.playlists[i]->name);
		for(struct playlist_item *j = gacfg.playlists[i]->head; j; j = j->next)
			vlog("  - %s", j->filename);
	}
#endif
	return ACE_NONE;
}

struct file_write_buffer {
	char buffer[128];
	size_t len;
	FILE *fp;
};

static int fb_flush(struct file_write_buffer *fb)
{
	int ret = fwrite(fb->buffer, fb->len, 1, fb->fp) == 1;
	fb->len = 0;
	return ret;
}

static int fb_write(struct file_write_buffer *fb, const char *b, size_t len)
{
	int ret = 1;
	if(fb->len + len > sizeof(fb->buffer))
	{
		if(!fb_flush(fb)) return 0;
		ret = fwrite(b, len, 1, fb->fp) == 1;
	}
	else
	{
		memcpy(fb->buffer + fb->len, b, len);
		fb->len += len;
	}
	return ret;
}

static int fb_write_string(struct file_write_buffer *fb, const char *s)
{
	return fb_write(fb, s, strlen(s));
}

static int fb_write_char(struct file_write_buffer *fb, char ch)
{
	return fb_write(fb, &ch, 1);
}

static int fb_close(struct file_write_buffer *fb)
{
	if(!fb_flush(fb)) return 0;
	return fclose(fb->fp) == 0;
}

int acfg_save(void)
{
	struct file_write_buffer fb = {
		.len = 0,
		.fp = fopen(ACONFIG_FILE, "w"),
	};
	if(!fb.fp) return ACE_FILE_IO;
	int err = ACE_NONE;
#define TRY(expr) do { if(!(expr)) { err = ACE_FILE_IO; goto out; } } while(0)

	TRY(fb_write_string(&fb,
		"##===============================================##\n"
		"## This configuration file was generated by Rune3DS  ##\n"
		"##  edit at your own risk.                       ##\n"
		"##===============================================##\n"
		"\n"
	));

	TRY(fb_write_string(&fb, "playlist_options = "));
	if(gacfg.default_playlist_options == 0) TRY(fb_write_string(&fb, "none"));
	else
	{
		int c = 0;
#define OPT(def,name) if(gacfg.default_playlist_options & def) { if(c) fb_write_string(&fb, ", "); fb_write_string(&fb, #name); c = 1; }
		OPT(SP_RANDOMISE, randomise);
		OPT(SP_REPEAT, repeat);
#undef OPT
		TRY(fb_write_char(&fb, '\n'));
	}

	TRY(fb_write_string(&fb, "mono = "));
	TRY(fb_write_string(&fb, gacfg.alwaysMono ? "yes\n" : "no\n"));

	TRY(fb_write_string(&fb, "default_playlist = "));
	TRY(fb_write_string(&fb, gacfg.default_playlist_name ? gacfg.default_playlist_name : "NULL"));
	TRY(fb_write_char(&fb, '\n')); /* second newline is done by the loop */

	for(int i = 0; i < gacfg.nplaylists; ++i)
	{
		TRY(fb_write_char(&fb, '\n'));
		TRY(fb_write_string(&fb, gacfg.playlists[i]->name));
		TRY(fb_write_string(&fb, " [\n"));
		for(struct playlist_item *j = gacfg.playlists[i]->head; j; j = j->next)
		{
			TRY(fb_write_char(&fb, '\t'));
			TRY(fb_write_string(&fb,
				strncmp(j->filename, ACONFIG_MUSIC_DIR, sizeof(ACONFIG_MUSIC_DIR) - 1) == 0
				? j->filename + sizeof(ACONFIG_MUSIC_DIR) - 1 : j->filename));
			TRY(fb_write_char(&fb, '\n'));
		}
		TRY(fb_write_string(&fb, "]\n"));
	}

out:
	return fb_close(&fb) ? err : ACE_FILE_IO;
#undef TRY
}

void acfg_free(void)
{
	free(gacfg.default_playlist_name);
	for(int i = 0; i < gacfg.nplaylists; ++i)
		playlist_free(gacfg.playlists[i]);
	free(gacfg.playlists);
	plist_exit();
}
