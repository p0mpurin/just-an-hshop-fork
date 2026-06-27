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
#include "audio/cwav_reader.h"
#include "audio/playlist.h"
#include "audio/player.h"
#include "audio_cfg.hh"
#include "panic.hh"
#include "i18n.hh"
#include "log.hh"

#include <ui/menuselect.hh>
#include <ui/checkbox.hh>
#include <ui/swkbd.hh>
#include <ui/base.hh>

#include <sys/types.h>
#include <dirent.h>
#include <string>
#include <vector>

#define MUSIC_DIR "/3ds/Rune3DS/music"

struct cwav_meta {
	std::string path, title, artist;
};
static std::vector<cwav_meta> cwav_meta_cache;
static ui::Button *saveButton;
static int dirty_flag = 0;


/* unused param so it can be put into the ui::CheckBox on_change callback */
static void set_dirty(bool unused = false)
{
	(void) unused;
	dirty_flag = 1;
	saveButton->set_hidden(false);
}

static void cache_meta()
{
	if(cwav_meta_cache.size())
		return;

	DIR *d = opendir(MUSIC_DIR);
	if(!d) return;

	struct dirent *ent;
	struct cwav cw;
	cwav_meta meta;
	while((ent = readdir(d)))
	{
		if(ent->d_type != DT_REG) continue;
		meta.path = MUSIC_DIR "/" + std::string(ent->d_name);
		if(cwav_init(&cw, meta.path.c_str()) != 0) continue;
		meta.title = cw.title;
		meta.artist = cw.artist ? cw.artist : "";
		cwav_meta_cache.push_back(meta);
		cwav_close(&cw);
	}

	closedir(d);
}

static void insert_meta_name(std::string& name, const cwav_meta& meta)
{
	if(meta.artist.size())
		name = meta.artist + " - " + meta.title;
	else
		name = meta.title;
}

static int show_select_file_internal()
{
	cache_meta();

	if(!cwav_meta_cache.size())
	{
		ui::notice(str::add_music);
		return -1;
	}

	ui::I18NEnabledRenderQueue rq;
	ui::MenuSelect *msel;
	int ret = -1;

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.when_select([&msel, &ret]() -> bool {
			ret = msel->pos();
			return false;
		})
		.add_to(&msel, rq);

	std::string name;
	for(cwav_meta& meta : cwav_meta_cache)
	{
		insert_meta_name(name, meta);
		msel->add_row(name);
	}

	rq.render_finite_button(KEY_B | KEY_START);
	return ret;
}

static void show_select_file()
{
	int pos = show_select_file_internal();
	if(pos >= 0)
		player_play(cwav_meta_cache[pos].path.c_str());
}

static void normalise_name(std::string& name)
{
	for(size_t i = 0; i < name.size(); ++i)
		if(name[i] == '-' || name[i] == '_')
			name[i] = ' ';
}

static int show_select_playlist_internal(const char *additional_label, u32 *keys)
{
	if(keys) *keys = 0;

	if(!acfg()->nplaylists && !additional_label)
	{
		ui::notice(str::add_playlist);
		return -1;
	}

	ui::I18NEnabledRenderQueue rq;
	ui::MenuSelect *msel;
	int ret = -1;

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.when_select([&msel, &ret, keys, additional_label]() -> bool {
			if(keys) *keys = msel->kdown_for_press();
			ret = msel->pos();
			/* can't press X on the additional label */
			if(additional_label && (msel->kdown_for_press() & KEY_X))
				return true;
			return false;
		})
		.keymask(keys ? KEY_X | KEY_A : KEY_A)
		.add_to(&msel, rq);

	int sel = 0;
	std::string name;
	for(int i = 0; i < acfg()->nplaylists; ++i)
	{
		name = acfg()->playlists[i]->name;
		if(plist_current())
			if(name == plist_current()->name)
				sel = i;
		normalise_name(name);
		msel->add_row(name);
	}
	if(additional_label)
		msel->add_row(additional_label);
	msel->set_pos(sel);

	rq.render_finite_button(KEY_B | KEY_START);
	return ret;
}

static void show_select_playlist()
{
	int pos = show_select_playlist_internal(NULL, NULL);
	if(pos >= 0)
	{
		struct playlist *pl = acfg()->playlists[pos];
		playlist_use(pl);
	}
}

static void show_manage_playlist_sub(struct playlist *pl)
{
	ui::MenuSelect *msel;
	ui::I18NEnabledRenderQueue rq;

	auto fill_msel = [&msel, pl]() -> void {
		msel->clear();
		std::string name;
		struct cwav cwav;
		for(struct playlist_item *pi = pl->head; pi; pi = pi->next)
		{
			if(cwav_init(&cwav, pi->filename) == 0)
			{
				if(cwav.artist)
					name = cwav.artist + std::string(" - ") + cwav.title;
				else
					name = cwav.title;
				msel->add_row(name);
				cwav_close(&cwav);
			}
			else
			{
				char *name = strrchr(pi->filename, '/');
				if(!name) name = pi->filename;
				else name += 1;
				msel->add_row(name);
			}
		}
		msel->add_row(str::add_file_act);
		msel->set_pos(0);
	};

	ui::builder<ui::MenuSelect>(ui::Screen::bottom)
		.when_select([&msel, pl, &fill_msel]() -> bool {
			if((int) msel->pos() == pl->size)
			{
				if(!(msel->kdown_for_press() & KEY_A))
					return true;
				/* add playlist */
				int meta_pos = show_select_file_internal();
				if(meta_pos >= 0)
				{
					playlist_append(pl, "", cwav_meta_cache[meta_pos].path.c_str());
					set_dirty();
					fill_msel();
				}
			}
			else if(msel->kdown_for_press() & KEY_X)
			{
				int i = 0;
				struct playlist_item *pi;
				for(pi = pl->head; pi && i != (int) msel->pos(); pi = pi->next)
					++i;
				panic_assert(pi, "unmapped index");
				dlog("unlinking %p (%s), i=%i", pi, pi->filename, i);
				playlist_unlink_item(pl, pi);
				set_dirty();
				fill_msel();
			}
			else if(msel->kdown_for_press() & KEY_L)
			{
				/* move up */
				if(msel->pos() != 0)
				{
					int new_pos = msel->pos() - 1;
					dlog("swapping %i with %lu", new_pos, msel->pos());
					playlist_swap(pl, msel->pos(), new_pos);
					set_dirty();
					fill_msel();
					msel->set_pos(new_pos);
				}
			}
			else if(msel->kdown_for_press() & KEY_R)
			{
				if((int) msel->pos() + 1 < pl->size)
				{
					int new_pos = msel->pos() + 1;
					dlog("swapping %i with %lu", new_pos, msel->pos());
					playlist_swap(pl, msel->pos(), new_pos);
					set_dirty();
					fill_msel();
					msel->set_pos(new_pos);
				}
			}
			return true;
		})
		.keymask(KEY_X | KEY_A | KEY_L | KEY_R)
		.add_to(&msel, rq);

	fill_msel();
	rq.render_finite_button(KEY_B | KEY_START);
}

static void show_add_playlist()
{
	SwkbdResult res;
	SwkbdButton btn;

	std::string name = ui::keyboard([](ui::AppletSwkbd *swkbd) -> void {
		swkbd->hint(STRING(new_playlist_name));
	}, &btn, &res);

	if(btn != SWKBD_BUTTON_CONFIRM || res == SWKBD_INVALID_INPUT || res == SWKBD_OUTOFMEM || res == SWKBD_BANNED_INPUT || !name.size())
		return;

	/* now we gotta validate the modify it to replace the spaces */
	for(size_t i = 0; i < name.size(); ++i)
		if(name[i] == ' ') name[i] = '-';

	struct playlist *pl = playlist_make(name.c_str());
	acfg_add_playlist(pl);
	set_dirty();
}

static void show_manage_playlists()
{
	u32 keys;
	int pos;

	while((pos = show_select_playlist_internal(STRING(add_playlist_act), &keys)) >= 0)
	{
		if(pos == acfg()->nplaylists)
			show_add_playlist();
		else if(keys & KEY_A)
			show_manage_playlist_sub(acfg()->playlists[pos]);
		else if(keys & KEY_X)
		{
			acfg_delete_playlist(pos);
			set_dirty();
		}
	}
}

void show_audio_config()
{
	ui::I18NEnabledRenderQueue rq;
	ui::CheckBox *opt_randomise, *opt_repeat, *opt_stereo;
	ui::Button *defaultPlaylist;
	ui::Text *hdr;
	int defaultPlaylistNone = !(acfg()->default_playlist_name);

	std::function<void()> commit_changes = [&opt_randomise, &opt_repeat, &opt_stereo, &defaultPlaylist, &defaultPlaylistNone]() -> void {
		acfg()->default_playlist_options =
			  (opt_randomise->checked() ? SP_RANDOMISE : 0)
			| (opt_repeat->checked() ? SP_REPEAT : 0);
		plist_set_flags(acfg()->default_playlist_options);
		acfg()->alwaysMono = !opt_stereo->checked();
		player_reconfigure_mix();
		free(acfg()->default_playlist_name);
		if(defaultPlaylistNone) acfg()->default_playlist_name = NULL;
		else acfg()->default_playlist_name = strdup(defaultPlaylist->get_label().c_str());
	};

	ui::builder<ui::Text>(ui::Screen::bottom, str::basic_options)
		.x(10.0f).y(10.0f).wrap()
		.add_to(&hdr, rq);

	ui::builder<ui::CheckBox>(ui::Screen::bottom, !!(plist_get_flags() & SP_RANDOMISE))
		.under(rq.back()).align_x(rq.back(), 3.0f)
		.when_change(set_dirty)
		.add_to(&opt_randomise, rq);
	ui::builder<ui::Text>(ui::Screen::bottom, str::randomize)
		.right(rq.back()).align_y(rq.back()).middle(rq.back())
		.size(0.5f)
		.add_to(rq);

	ui::builder<ui::CheckBox>(ui::Screen::bottom, !!(plist_get_flags() & SP_REPEAT))
		.under(rq.back()).align_x(opt_randomise)
		.when_change(set_dirty)
		.add_to(&opt_repeat, rq);
	ui::builder<ui::Text>(ui::Screen::bottom, str::repeat)
		.right(rq.back()).align_y(rq.back()).middle(rq.back())
		.size(0.5f)
		.add_to(rq);

	ui::builder<ui::CheckBox>(ui::Screen::bottom, !acfg()->alwaysMono)
		.under(rq.back()).align_x(opt_randomise)
		.when_change(set_dirty)
		.add_to(&opt_stereo, rq);
	ui::builder<ui::Text>(ui::Screen::bottom, str::stereo)
		.right(rq.back()).align_y(rq.back()).middle(rq.back())
		.size(0.5f)
		.add_to(rq);

	ui::builder<ui::Text>(ui::Screen::bottom, str::default_playlist)
		.under(rq.back(), 5.0f).align_x(hdr).wrap()
		.add_to(rq);
	ui::builder<ui::Button>(ui::Screen::bottom, acfg()->default_playlist_name ? acfg()->default_playlist_name : STRING(none))
		.manual_i18n_update([](ui::Button *b, lang::type) -> void {
			b->set_label(acfg()->default_playlist_name ? acfg()->default_playlist_name : STRING(none));
		})
		.size(ui::screen_width(ui::Screen::bottom), 20.0f)
		.when_clicked([&defaultPlaylistNone, &defaultPlaylist](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then([&defaultPlaylistNone, &defaultPlaylist]() -> void {
				int pos = show_select_playlist_internal(NULL, NULL);
				if(pos == -1)
				{
					if(!defaultPlaylistNone)
						set_dirty();
					defaultPlaylistNone = 1;
					defaultPlaylist->set_label(str::none);
				}
				else if(pos >= 0)
				{
					set_dirty();
					defaultPlaylistNone = 0;
					defaultPlaylist->set_label(acfg()->playlists[pos]->name);
				}
			});
			return true;
		})
		.under(rq.back())
		.add_to(&defaultPlaylist, rq);

	ui::builder<ui::Text>(ui::Screen::bottom, str::other)
		.under(rq.back(), 5.0f).align_x(hdr).wrap()
		.add_to(rq);

	ui::builder<ui::Text>(ui::Screen::bottom, str::select)
		.size(0.60f)
		.under(rq.back()).align_x(opt_randomise)
		.add_to(rq);

	ui::builder<ui::Button>(ui::Screen::bottom, str::playlist)
		.when_clicked([](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then(show_select_playlist);
			return true;
		})
		.size_children(0.55f)
		.wrap()
		.under(rq.back()).align_x(rq.back(), 10.0f)
		.add_to(rq);
	ui::builder<ui::Button>(ui::Screen::bottom, str::file)
		.when_clicked([](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then(show_select_file);
			return true;
		})
		.size_children(0.55f)
		.wrap()
		.under(rq.back()).align_x(rq.back())
		.add_to(rq);

	ui::builder<ui::Button>(ui::Screen::bottom, str::manage_playlists)
		.when_clicked([](ui::Button *) -> bool {
			ui::RenderQueue::global()->render_and_then(show_manage_playlists);
			return true;
		})
		.wrap()
		.under(rq.back(), 4.0f).align_x(opt_randomise)
		.add_to(rq);

	ui::builder<ui::Button>(ui::Screen::bottom, str::save)
		.when_clicked([&commit_changes](ui::Button *b) -> bool {
			commit_changes();
			acfg_save();
			dirty_flag = 0;
			b->set_hidden(true);
			return true;
		})
		.wrap()
		.y(ui::layout::bottom).x(ui::layout::right)
		.add_to(&saveButton, rq);

	saveButton->set_hidden(!dirty_flag);

	rq.render_finite_button(KEY_B | KEY_START);

	commit_changes();
}
