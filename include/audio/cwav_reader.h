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

#ifndef inc_cwav_reader_h
#define inc_cwav_reader_h

#ifdef __cplusplus
extern "C" {
#endif

#include <3ds/types.h>
#include <3ds/ndsp/ndsp.h>
#include <3ds/ndsp/channel.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CWAV_NDSP_ENCODING(cw) (NDSP_CHANNELS(1) | NDSP_ENCODING((cw)->encoding == CWAV_ENC_IMA_ADPCM ? CWAV_ENC_PCM16 : (cw)->encoding))

enum cwav_channel {
	CWAV_LEFT  = 0,
	CWAV_RIGHT = 1,
};

enum cwav_encoding {
	CWAV_ENC_PCM8      = 0, ///< Supported by ndsp
	CWAV_ENC_PCM16     = 1, ///< Supported by ndsp
	CWAV_ENC_DSP_ADPCM = 2, ///< Supported by ndsp
	CWAV_ENC_IMA_ADPCM = 3, ///< Not supported by ndsp
};

struct cwav {
	int nchannels;
	float rate;
	uint8_t encoding;
	ndspAdpcmData dspAdpcmContext[2];
	/* hwav metadata, all optional */
	char *artist;
	char *title;
	/* internal use */
	FILE *sourceFile;
	size_t fileLength;
	uint32_t endFrame, loopPoint;
	uint32_t dataRef;
	uint32_t samplesOffsets[2];
	uint32_t frameCounters[2];
	union {
		struct {
			uint16_t param[16];
			ndspAdpcmData context;
			ndspAdpcmData contextLoop;
			ndspAdpcmData context0;
		} DSP_ADPCM[2];
		struct {
			uint16_t predictorLoop, predictor0, predictor;
			int8_t step_indexLoop, step_index0, step_index;
		} IMA_ADPCM[2];
	};
};


size_t cwav_read(struct cwav *cw, int channel, void *samples, unsigned bytesize);
int cwav_init(struct cwav *cw, const char *src);
void cwav_to_looppoint(struct cwav *cw);
int cwav_can_read(struct cwav *cw);
void cwav_close(struct cwav *cw);
void cwav_to_0(struct cwav *cw);

static inline int cwav_samples_read(struct cwav *cw, int channel)
{
	return cw->frameCounters[channel];
}

#ifdef __cplusplus
}
#endif

#endif

