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


#include <string.h>
#include <stdlib.h>
#include <3ds.h>
#include "audio/configuration.h"
#include "audio/cwav_reader.h"
#include "audio/playlist.h"
#include "audio/player.h"
#include "error.hh"

#include "log.hh"

#define PLAYER_BUFFER_SIZE 0x8000

static Thread playerThread;
static enum {
	CMD_NONE           = 0,
	CMD_UNPAUSE        = 1,
	CMD_PAUSE          = 2,
	CMD_EXIT           = 3,
	CMD_MIXCONF        = 4,
	CMD_IS_PAUSED      = 5, /* out:b */
	CMD_PLAY           = 6, /* in:string */
	CMD_PREV           = 7,
	CMD_NEXT           = 8,
	CMD_RESTART_PLIST  = 9,
	CMD_PUSH           = 10, /* TODO: Push the current audio file to some backup variable */
	CMD_POP            = 11, /* TODO: Pop the backup variable and continue playing */
	CMD_INIT           = 12,
	CMD_HALT           = 13,
	CMD_COIFPLAY       = 14, /* in:vptr */
} playerCommand;
union {
	char *string;
	void *vptr;
	u32 uint32;
	s32 int32;
	bool b;
} playerCommandArg;
static int shouldFireFinishEvent = 0;
static LightEvent commandFinishedEvent;
static LightLock fireLock;
enum {
	ACMD_NONE,
	ACMD_REPLENISH,
};
struct audiocb_data {
	ndspWaveBuf (*buffers)[2][2];
	int *audioCmd, *bufsel;
};
static LightEvent playerEvent;
static enum {
	SOURCE_STOPPED,
	SOURCE_PLIST,
	SOURCE_DIRECT,
} source = SOURCE_STOPPED;
static player_switch_callback switch_callback = NULL;
static struct playlist_item *current_item = NULL;

#define makemix_p(p1, p2, p3, p4) {p1,p2,p3,p4,p1,p2,p3,p4,p1,p2,p3,p4}
#define makemix(isRight) makemix_p(1 - isRight, isRight, 1 - isRight, isRight)
static float playerChannelMixes[2][2][12] = { { makemix_p(1,1,1,1), makemix_p(1,1,1,1) }, { makemix(0), makemix(1) } };
#undef makemix_p
#undef makemix


static int player_replenish_buffers(struct cwav *cw, ndspWaveBuf buffers[2][2], int j)
{
	size_t nsamples = cwav_read(cw, CWAV_LEFT, buffers[0][j].data_pcm8, PLAYER_BUFFER_SIZE);
	if(nsamples == 0) return -1;
	buffers[0][j].nsamples = nsamples;

	if(cw->nchannels == 2)
	{
		nsamples = cwav_read(cw, CWAV_RIGHT, buffers[1][j].data_pcm8, PLAYER_BUFFER_SIZE);
		if(nsamples == 0) return -1;
		buffers[1][j].nsamples = nsamples;
	}

	/* add the buffers at about the same time (important for first buffer) */
	DSP_FlushDataCache(&buffers[0][j].data_pcm8, PLAYER_BUFFER_SIZE);
	DSP_FlushDataCache(&buffers[1][j].data_pcm8, PLAYER_BUFFER_SIZE);
	ndspChnWaveBufAdd(CWAV_LEFT, &buffers[0][j]);
	if(cw->nchannels == 2)
		ndspChnWaveBufAdd(CWAV_RIGHT, &buffers[1][j]);

	return 0;
}

static void configure_mix_for_channel(struct cwav *cw, int i)
{
	int mixConfiguration = acfg()->alwaysMono ? 0 : cw->nchannels - 1;
	ndspChnSetMix(i, playerChannelMixes[mixConfiguration][i]);
}

static void player_initialize_buffers_with_cwav(struct cwav *cw, ndspWaveBuf buffers[2][2])
{
	for(int i = 0; i < 2; ++i)
		for(int j = 0; j < 2; ++j)
		{
			if(cw->encoding == CWAV_ENC_DSP_ADPCM)
				buffers[i][j].adpcm_data = &cw->dspAdpcmContext[i];
			else
				buffers[i][j].adpcm_data = NULL;
			buffers[i][j].offset = 0;
			buffers[i][j].looping = false;
			buffers[i][j].status = NDSP_WBUF_DONE;
		}

	/* initialize the ndsp channels */

	for(int i = 0; i < cw->nchannels; ++i)
	{
		ndspChnReset(i);
		/* setup the channel */
		ndspChnSetRate(i, cw->rate);
		ndspChnSetFormat(i, CWAV_NDSP_ENCODING(cw));
		configure_mix_for_channel(cw, i);
		/* adpcm: we gotta set this field */
		if(cw->encoding == CWAV_ENC_DSP_ADPCM)
			ndspChnSetAdpcmCoefs(i, cw->DSP_ADPCM[i].param);
	}

	/* prepare first data batch */
	player_replenish_buffers(cw, buffers, 0);
	player_replenish_buffers(cw, buffers, 1);
}

static void player_ndsp_callback(void *prv_data)
{
	struct audiocb_data *data = (struct audiocb_data *) prv_data;

	if((*data->buffers)[0][*data->bufsel].status == NDSP_WBUF_DONE && (*data->buffers)[1][*data->bufsel].status == NDSP_WBUF_DONE)
	{
		/* ran on the "main audio thread" because it has a higher priority and it won't block the player thread */
		*data->audioCmd = ACMD_REPLENISH;
		LightEvent_Signal(&playerEvent);
	}
}

static void player_set_pause(bool p)
{
	ndspChnSetPaused(CWAV_LEFT, p);
	ndspChnSetPaused(CWAV_RIGHT, p);
}

static void player_reset(void)
{
	ndspChnReset(CWAV_LEFT);
	ndspChnReset(CWAV_RIGHT);
}

static void player_stop(void)
{
	player_reset();
	source = SOURCE_STOPPED;
}

static void player_loop_cwav(struct cwav *cw, ndspWaveBuf buffers[2][2])
{
	cwav_to_looppoint(cw);
	player_reset();
	player_initialize_buffers_with_cwav(cw, buffers);
}

static void player_replay_cwav(struct cwav *cw, ndspWaveBuf buffers[2][2])
{
	cwav_to_0(cw);
	player_reset();
	player_initialize_buffers_with_cwav(cw, buffers);
}

static int player_play_file(struct cwav *currentFile, ndspWaveBuf buffers[2][2], const char *s)
{
	struct cwav loadingFile;
	player_reset();
	if(cwav_init(&loadingFile, s) == 0)
	{
		cwav_close(currentFile);
		*currentFile = loadingFile;
		player_initialize_buffers_with_cwav(currentFile, buffers);
		if(switch_callback) switch_callback(currentFile);
		return 1;
	}
	else
	{
		elog("failed to load cwav \"%s\"!", s);
		return 0;
	}
}

static void try_play_first_until_end(struct cwav *currentFile, ndspWaveBuf buffers[2][2], struct playlist_item *(func)(void), int is_start)
{
	if(!is_start && plist_1sized_list())
	{
		if(plist_get_flags() & SP_REPEAT)
			player_loop_cwav(currentFile, buffers);
		else
			player_stop();
	}
	else
	{
		for(int tried = 0; tried != plist_current()->size; ++tried)
		{
			struct playlist_item *cit = func();
			current_item = cit;
			dlog("next in attempt list: %s", cit ? cit->filename : "(null)");
			/* playlist is done */
			if(!cit) { player_stop(); return; }
			/* we made the file play! lets stop */
			else if(player_play_file(currentFile, buffers, cit->filename))
				return;
		}
		/* if we walked all playlist items and found none the playlist is entirely busted */
		elog("busted playlist! %s", plist_current()->name);
		player_stop();
	}
}

static void player_next_plist(struct cwav *currentFile, ndspWaveBuf buffers[2][2])
{
	try_play_first_until_end(currentFile, buffers, plist_next, 0);
}

static void player_start_plist(struct cwav *currentFile, ndspWaveBuf buffers[2][2])
{
	try_play_first_until_end(currentFile, buffers, plist_next, 1);
}

static void player_prev_plist(struct cwav *currentFile, ndspWaveBuf buffers[2][2])
{
	/* if we've played more than ~5 seconds of audio then we'll replay the file */
	if(((float) cwav_samples_read(currentFile, CWAV_LEFT) / currentFile->rate) > 5.0f)
		player_replay_cwav(currentFile, buffers);
	/* else we'll play the previous file */
	else
		try_play_first_until_end(currentFile, buffers, plist_prev, 0);
}

static void player_thread_entrypoint(void *arg)
{
	(void) arg;
	uint8_t *backingBuffer = (uint8_t *)linearAlloc(4 * PLAYER_BUFFER_SIZE);
	ndspWaveBuf buffers[2][2] = { 0 };
	struct cwav currentFile;
	int audioCmd = ACMD_NONE;
	int bufsel = 0;
	Result ndspInitRes = -1;

	/* if not currentFile may be used uninitialized */
	memset(&currentFile, 0, sizeof(currentFile));

	if (!backingBuffer)
		return;	/* failure to allocate; just don't play at all */

	/* set the buffer pointer for each of our sub-buffers */
	for(int i = 0; i < 2; i++)
		for (int j = 0; j < 2; j++)
			buffers[i][j].data_vaddr = &backingBuffer[PLAYER_BUFFER_SIZE * (i * 2 + j)];

	if (R_FAILED(ndspInit()))
		goto fail1;

	struct audiocb_data cbData = { .buffers = &buffers, .audioCmd = &audioCmd, .bufsel = &bufsel };
	ndspSetCallback(player_ndsp_callback, &cbData);

	while(playerCommand != CMD_EXIT)
	{
		switch(audioCmd)
		{
		case ACMD_REPLENISH:
			if(source == SOURCE_STOPPED)
				break;
			if(cwav_can_read(&currentFile))
			{
				player_replenish_buffers(&currentFile, buffers, bufsel);
				bufsel ^= 1;
			}
			else if(buffers[0][bufsel ^ 1].status == NDSP_WBUF_DONE && buffers[0][bufsel ^ 1].status == NDSP_WBUF_DONE)
			{
				/* finished playing, try playing the next file */
				if(source == SOURCE_PLIST)
					player_next_plist(&currentFile, buffers);
				/* replay the file from player_play() if needed */
				else if(source == SOURCE_DIRECT)
				{
					if(plist_get_flags() & SP_REPEAT)
						player_loop_cwav(&currentFile, buffers);
					else
						player_stop();
				}
			}
			break;

		case ACMD_NONE:
			break;
		}

		switch(playerCommand)
		{
		case CMD_UNPAUSE:
			if(source != SOURCE_STOPPED)
				player_set_pause(false);
			break;
		case CMD_PAUSE:
			if(source != SOURCE_STOPPED)
				player_set_pause(true);
			break;
		case CMD_MIXCONF:
			if(source != SOURCE_STOPPED)
				for(int i = 0; i < currentFile.nchannels; ++i)
					configure_mix_for_channel(&currentFile, i);
			break;
		case CMD_IS_PAUSED:
			playerCommandArg.b = ndspChnIsPaused(CWAV_LEFT) && ndspChnIsPaused(CWAV_RIGHT);
			break;
		case CMD_PLAY:
			source = SOURCE_DIRECT;
			/* we can discard the "is success" return here, since it does not matter */
			player_play_file(&currentFile, buffers, playerCommandArg.string);
			free(playerCommandArg.string);
			break;
		case CMD_RESTART_PLIST:
			source = SOURCE_PLIST;
			player_start_plist(&currentFile, buffers);
			break;
		case CMD_NEXT:
			if(source == SOURCE_PLIST)
				player_next_plist(&currentFile, buffers);
			break;
		case CMD_PREV:
			if(source == SOURCE_PLIST)
				player_prev_plist(&currentFile, buffers);
			else if(source == SOURCE_DIRECT) /* if the source was player_play() always replay on previous */
				player_replay_cwav(&currentFile, buffers);
			break;
		case CMD_INIT:
			/* does not actually do anything, just ensures player_init() does not
			 *  terminate until this thread is fully set up and listening for commands */
			break;
		case CMD_PUSH:
		case CMD_POP:
			break; /* stubbed */
		case CMD_HALT:
			player_stop();
			break;
		case CMD_COIFPLAY:
			if(current_item == playerCommandArg.vptr)
				player_next_plist(&currentFile, buffers);
			break;
		/* shouldn't happen */
		case CMD_EXIT:
			continue;
		case CMD_NONE:
			break;
		}

		audioCmd = ACMD_NONE;
		if(playerCommand != CMD_NONE)
		{
			/* */ LightLock_Lock(&fireLock); /* */
			playerCommand = CMD_NONE;
			if(shouldFireFinishEvent)
				LightEvent_Signal(&commandFinishedEvent);
			/* */ LightLock_Unlock(&fireLock); /* */
		}
		LightEvent_Wait(&playerEvent);
	}

	player_reset();
	ndspExit();

fail1:
	linearFree(backingBuffer);
}

static void player_wake(void);
static void player_wait(void);
Result player_init(void)
{
	s32 prio;
	Result res;

	if(R_FAILED(res = svcGetThreadPriority(&prio, CUR_THREAD_HANDLE)))
		return res;

	LightEvent_Init(&commandFinishedEvent, RESET_ONESHOT);
	LightEvent_Init(&playerEvent, RESET_ONESHOT);
	LightLock_Init(&fireLock);

	LightLock_Lock(&fireLock);

	if(!(playerThread = threadCreate(player_thread_entrypoint, NULL, 64 * 1024, prio - 2, -2, false)))
		return APPERR_OUT_OF_MEM;

	playerCommand = CMD_INIT;
	LightLock_Unlock(&fireLock);
	player_wait();

	return 0;
}

static void player_wait(void)
{
	/* */ LightLock_Lock(&fireLock); /* */
	if(playerCommand != CMD_NONE)
	{
		shouldFireFinishEvent = 1;
		/* */ LightLock_Unlock(&fireLock); /* */
		LightEvent_Wait(&commandFinishedEvent);
		/* */ LightLock_Lock(&fireLock); /* */
		shouldFireFinishEvent = 0;
	}
	/* */ LightLock_Unlock(&fireLock); /* */
}

static void player_wake(void)
{
	LightEvent_Signal(&playerEvent);
}

void player_exit(void)
{
	if(!playerThread)
		return;
	player_wait();
	playerCommand = CMD_EXIT;
	player_wake();

	threadJoin(playerThread, U64_MAX);
	threadFree(playerThread);
	playerThread = NULL;
}

#define MAKE_PLAYER_COMMAND(name, cmd_name, additional_setup, ...) \
	void name(__VA_ARGS__) { \
		dlog("Waiting to submit command " #cmd_name); \
	  player_wait(); \
	  playerCommand = cmd_name; \
	  additional_setup; \
	  player_wake(); \
	}

MAKE_PLAYER_COMMAND(player_pause, CMD_PAUSE,)
MAKE_PLAYER_COMMAND(player_unpause, CMD_UNPAUSE,)
MAKE_PLAYER_COMMAND(player_play, CMD_PLAY, playerCommandArg.string = strdup(filename), const char *filename)
MAKE_PLAYER_COMMAND(player_next, CMD_NEXT,)
MAKE_PLAYER_COMMAND(player_previous, CMD_PREV,)
MAKE_PLAYER_COMMAND(player_refresh_playlist, CMD_RESTART_PLIST,)
MAKE_PLAYER_COMMAND(player_reconfigure_mix, CMD_MIXCONF,)
MAKE_PLAYER_COMMAND(player_halt, CMD_HALT,)
MAKE_PLAYER_COMMAND(player_continue_if_playing, CMD_COIFPLAY, playerCommandArg.vptr = pi, struct playlist_item *pi)

bool player_is_paused(void)
{
	player_wait();
	playerCommand = CMD_IS_PAUSED;
	player_wake();
	player_wait();
	return playerCommandArg.b;
}

void player_wait_commands(void)
{
	while(playerCommand != CMD_NONE)
		player_wait();
}

void player_set_switch_callback(player_switch_callback cb)
{
	switch_callback = cb;
}

