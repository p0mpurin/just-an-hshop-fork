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

#include "audio/cwav_reader.h"
#include <string.h>
#include <stdlib.h>
#include "log.hh"

/* -- broken currently, will probably be fixed in a later version
 * works now, define still here due to lazyness */
#define CWAV_ENABLE_ADPCM 1

/* Reference: https://3dbrew.org/wiki/BCWAV */

enum cwav_typeid {
	typeid_dsp_adpcm_info = 0x0300,
	typeid_ima_adpcm_info = 0x0301,
	typeid_sample_data    = 0x1F00,
	typeid_info_block     = 0x7000,
	typeid_data_block     = 0x7001,
	typeid_channel_info   = 0x7100,
	typeid_vorbis_comment = 0x8000,
};

struct cwav_adpcm_context {
	uint16_t index;
	uint16_t prevSample;
	uint16_t dprevSample;
} __attribute__((packed));

struct cwav_reference {
	uint16_t typeId;
	uint16_t padding1;
	uint32_t offset;
} __attribute__((packed));

struct cwav_sized_reference {
	uint16_t typeId;
	uint16_t padding2;
	uint32_t offset;
	uint32_t size;
} __attribute__((packed));

struct cwav_header {
	char magic[4]; /* "CWAV" */
	uint16_t endian;
	uint16_t headerSize;
	uint32_t version;
	uint32_t fileSize;
	uint16_t nblocks;
	uint16_t reserved1;
	struct cwav_sized_reference infoRef;
	struct cwav_sized_reference dataRef;
} __attribute__((packed));

struct hwav_header {
	char magic[4]; /* "HWAV" */
	uint16_t extendedBlockCount;
	struct cwav_sized_reference extendedBlocks[];
} __attribute__((packed));

struct cwav_info_block {
	char magic[4]; /* "INFO" */
	uint32_t size;
	uint8_t encoding;
	uint8_t loop;
	uint16_t padding3;
	uint32_t sampleRate;
	uint32_t loopStartFrame;
	uint32_t loopEndFrame;
	uint32_t reserved2;
	uint32_t nrefs;
	struct cwav_reference refs[]; /* to channel info objects */
} __attribute__((packed));

struct cwav_dsp_adpcm_info {
	uint16_t coefficients[16];
	struct cwav_adpcm_context context;
	struct cwav_adpcm_context contextLoop;
	uint16_t padding4;
} __attribute__((packed));

struct cwav_ima_adpcm_info {
	int16_t predictor;
	uint8_t step_index;
	uint8_t padding5;
	int16_t predictorLoop;
	uint8_t step_indexLoop;
	uint8_t padding6;
} __attribute__((packed));

struct cwav_channel_info {
	struct cwav_reference samplesRef;
	struct cwav_reference adpcmInfoRef; /* either cwav_dsp_adpcm_info or cwav_ima_adpcm_info */
	uint32_t reserved3;
} __attribute__((packed));


static int validate_sref(struct cwav *cw, struct cwav_sized_reference *r)
{
	return cw->fileLength >= (r->offset + r->size);
}

static char *dupstrn(const char *buffer, size_t len)
{
	char *ret = (char *)malloc(len + 1);
	if(!ret) return NULL;
	memcpy(ret, buffer, len);
	ret[len] = '\0';
	return ret;
}

#if CWAV_ENABLE_ADPCM
static unsigned align_14_down(unsigned n)
{
	if(n < 14) return n;
	return n - (n % 14);
}
#endif

int cwav_init(struct cwav *cw, const char *src)
{
	FILE *f;
	char infoDataBuffer[0xC0];
	struct cwav_info_block *info = (struct cwav_info_block *) infoDataBuffer;
	struct cwav_sized_reference href;
	struct cwav_channel_info *cinf;
	struct hwav_header hhdr;
	struct cwav_header hdr;
#if CWAV_ENABLE_ADPCM
	struct cwav_dsp_adpcm_info *dspAdpcmInfo;
	struct cwav_ima_adpcm_info *imaAdpcmInfo;
#endif

	vlog("Loading CWAV: %s", src);

	f = fopen(src, "r");
	if(!f) return 1;
	cw->sourceFile = f;
	fseek(f, 0, SEEK_END);
	cw->fileLength = ftell(f);
	fseek(f, 0, SEEK_SET);

	cw->artist = cw->title = NULL;

	if(fread(&hdr, sizeof(hdr), 1, f) != 1) goto fail;
	if(memcmp(hdr.magic, "CWAV", 4) != 0) goto fail;
	/* only LE is supported (for now?) */
	if(hdr.endian != 0xFEFF) goto fail;
	if(hdr.fileSize != cw->fileLength) goto fail;
	if(hdr.headerSize != 0x40) goto fail;
	if(hdr.version != 0x02010000) goto fail;
	if(hdr.nblocks != 2) goto fail;
	if(!validate_sref(cw, &hdr.infoRef) || hdr.infoRef.typeId != typeid_info_block)
		goto fail;
	if(!validate_sref(cw, &hdr.dataRef) || hdr.dataRef.typeId != typeid_data_block)
		goto fail;
	/* now we test for the HWAV header, which is always located at 0x40 if it exists (directly after the CWAV header + padding) */
	fseek(f, 0x40, SEEK_SET);
	if(fread(&hhdr, sizeof(hhdr), 1, f) != 1)
		goto fail;
	if(memcmp(hhdr.magic, "HWAV", 4) == 0)
	{
		vlog("HWAV detected.");

		/* great, this is a HWAV file */
		for(int i = 0; i < hhdr.extendedBlockCount; ++i)
		{
			fseek(f, 0x40 + sizeof(hhdr) + sizeof(href) * i, SEEK_SET);
			if(fread(&href, sizeof(href), 1, f) != 1)
				goto fail;
			if(!validate_sref(cw, &hdr.dataRef))
				goto fail;
			fseek(f, href.offset, SEEK_SET);
			switch(href.typeId)
			{
			case typeid_vorbis_comment:
			{
				vlog("parsing vorbis comment....");

				char *rbuffer = (char *)malloc(href.size), *ptr, *eq, *buffer;
				uint32_t len, fulllen, taglen, commentDataSize = 0;

				if(!rbuffer) break;
				if(fread(rbuffer, href.size, 1, f) != 1)
					goto failA;

				if(memcmp(rbuffer, "VCOM", 4) || * (uint32_t *) (&rbuffer[4]) != href.size)
					goto failA;
				buffer = rbuffer + 8;
				href.size -= 8;

				/* vendor length; we just skip it */
				len = * (uint32_t *) buffer;
				if(len > href.size) goto failA;
				ptr = buffer + len + sizeof(uint32_t);
				/* user tag count */
				len = * (uint32_t *) ptr;
				ptr += sizeof(uint32_t);
				href.size -= len + 2*sizeof(uint32_t);
				for(uint32_t i = 0; i < len; ++i)
				{
					fulllen = * (uint32_t *) ptr;
					commentDataSize += fulllen;
					if(commentDataSize > href.size)
						goto failA;
					ptr += sizeof(uint32_t);
					eq = (char *)memchr(ptr, '=', fulllen);
					if(!eq) goto failA;
					*eq = '\0'; ++eq;
					taglen = fulllen - (eq - ptr);
					dlog("found tag: |%s|=|%.*s|", ptr, (int) taglen, eq);
					if(strcasecmp(ptr, "artist") == 0)
						cw->artist = dupstrn(eq, taglen);
					else if(strcasecmp(ptr, "title") == 0)
						cw->title = dupstrn(eq, taglen);
					ptr += fulllen;
				}
failA:
				free(rbuffer);
				break;
			}
			/* unknown extended block; just ignore it */
			default:
				break;
			}
		}
	}
	/* 0xC0 is the size for 2 channels (ADPCM), so the biggest it'll get afaik */
	if(hdr.infoRef.size > sizeof(infoDataBuffer))
		goto fail;
	fseek(f, hdr.infoRef.offset, SEEK_SET);
	if(fread(infoDataBuffer, hdr.infoRef.size, 1, f) != 1)
		goto fail;
	if(memcmp(info->magic, "INFO", 4) != 0) goto fail;
	if(info->size != hdr.infoRef.size) goto fail;
	if(info->nrefs != 1 && info->nrefs != 2) goto fail;
	if(info->encoding != CWAV_ENC_PCM8 && info->encoding != CWAV_ENC_PCM16
#if CWAV_ENABLE_ADPCM
	&& info->encoding != CWAV_ENC_DSP_ADPCM && info->encoding != CWAV_ENC_IMA_ADPCM
#endif
		) goto fail;
	/* data should be at least that large */
	if(hdr.dataRef.size < info->loopStartFrame * info->nrefs * sizeof(int16_t))
		goto fail;

	for(size_t i = 0; i < info->nrefs; ++i)
	{
		/* offset is already validated earlier */
		if(info->refs[i].offset + sizeof(struct cwav_channel_info) > info->size || info->refs[i].typeId != typeid_channel_info)
			goto fail;
		cinf = (struct cwav_channel_info *) &((uint8_t *) &info->nrefs)[info->refs[i].offset];
		if(cinf->samplesRef.typeId != typeid_sample_data || cinf->samplesRef.offset > hdr.dataRef.size)
			goto fail;
		cw->samplesOffsets[i] = cinf->samplesRef.offset;

#if CWAV_ENABLE_ADPCM
		if(info->encoding == CWAV_ENC_DSP_ADPCM || info->encoding == CWAV_ENC_IMA_ADPCM)
		{
			if(info->encoding == CWAV_ENC_DSP_ADPCM)
			{
				if(cinf->adpcmInfoRef.offset + sizeof(struct cwav_dsp_adpcm_info) > info->size || cinf->adpcmInfoRef.typeId != typeid_dsp_adpcm_info)
					goto fail;

				dspAdpcmInfo = (struct cwav_dsp_adpcm_info *) &((uint8_t *) &info->nrefs)[info->refs[i].offset + cinf->adpcmInfoRef.offset];
				/* DSP ADPCM decoding info */
				memcpy(cw->DSP_ADPCM[i].param, dspAdpcmInfo->coefficients, sizeof(dspAdpcmInfo->coefficients));
				_Static_assert(sizeof(cw->DSP_ADPCM[i].context) == sizeof(dspAdpcmInfo->context), "compiler bad");
				memcpy(&cw->dspAdpcmContext[i], &dspAdpcmInfo->context, sizeof(dspAdpcmInfo->context));
				memcpy(&cw->DSP_ADPCM[i].context, &dspAdpcmInfo->context, sizeof(dspAdpcmInfo->context));
				memcpy(&cw->DSP_ADPCM[i].context0, &dspAdpcmInfo->context, sizeof(dspAdpcmInfo->contextLoop));
				memcpy(&cw->DSP_ADPCM[i].contextLoop, &dspAdpcmInfo->contextLoop, sizeof(dspAdpcmInfo->contextLoop));
			}
			else
			{
				if(cinf->adpcmInfoRef.offset + sizeof(struct cwav_ima_adpcm_info) > info->size || cinf->adpcmInfoRef.typeId != typeid_ima_adpcm_info)
					goto fail;

				imaAdpcmInfo = (struct cwav_ima_adpcm_info *) &((uint8_t *) &info->nrefs)[info->refs[i].offset + cinf->adpcmInfoRef.offset];
				/* IMA ADPCM decoding info */
				cw->IMA_ADPCM[i].step_index = cw->IMA_ADPCM[i].step_index0 = imaAdpcmInfo->step_index;
				cw->IMA_ADPCM[i].predictor = cw->IMA_ADPCM[i].predictor0 = imaAdpcmInfo->predictor;
				cw->IMA_ADPCM[i].step_indexLoop = imaAdpcmInfo->step_indexLoop;
				cw->IMA_ADPCM[i].predictorLoop = imaAdpcmInfo->predictorLoop;
			}
		}
#endif
	}
	cw->dataRef = hdr.dataRef.offset + 8;
	cw->nchannels = info->nrefs;
	cw->rate = (float) info->sampleRate;
	cw->encoding = info->encoding;
	cw->endFrame = info->loopEndFrame;
	cw->frameCounters[0] = 0;
	cw->frameCounters[1] = 0;
	cw->loopPoint = info->loopStartFrame;

	/* if we do not have a title yet then we generate one based on the filename */
	if(!cw->title)
	{
		const char *start = strrchr(src, '/'), *end = strrchr(src, '.');
		if(!start) start = src;
		else       start = start + 1; /* skip the slash */
		if(end && end > start)
			cw->title = dupstrn(start, end - start);
		else
			cw->title = strdup(start);
		/* replace [_-] with ' ' */
		for(size_t i = 0; cw->title[i]; ++i)
			if(cw->title[i] == '-' || cw->title[i] == '_')
				cw->title[i] = ' ';
	}

#if CWAV_ENABLE_ADPCM
	/* just to ensure cwav_read() returns the correct thing */
	if(cw->encoding == CWAV_ENC_DSP_ADPCM)
		cw->loopPoint = align_14_down(cw->loopPoint);
#endif

	return 0;
fail:
	cwav_close(cw);
	return 1;
}

void cwav_close(struct cwav *cw)
{
	fclose(cw->sourceFile);
	free(cw->artist);
	free(cw->title);
}

#if CWAV_ENABLE_ADPCM
static unsigned dsp_to_bytes(unsigned samples)
{
	return (samples / 14) * 8;
}

static unsigned dsp_to_bytes_align(unsigned samples)
{
	/* align up */
	if(samples % 14) samples += 14 - (samples % 14);
	return dsp_to_bytes(samples);
}

static unsigned dsp_to_samples(unsigned bytes)
{
	return (bytes / 8) * 14;
}
#endif

size_t cwav_read(struct cwav *cw, int channel, void *samples, unsigned bytesize)
{
	size_t samples_left = cw->endFrame - cw->frameCounters[channel];
	size_t ret = 0;

	/* PCM16 and PCM8 are fairly straight-forward to decode */
	if(cw->encoding == CWAV_ENC_PCM16 || cw->encoding == CWAV_ENC_PCM8)
	{
		int samplesize = cw->encoding == CWAV_ENC_PCM16 ? 2 : 1;
		size_t maxsamples = bytesize / samplesize;

		int ctr = cw->frameCounters[channel];
		int idealSamples = maxsamples > samples_left ? samples_left : maxsamples;
		uint32_t offset = cw->samplesOffsets[channel] + (ctr * samplesize);
		if(fseek(cw->sourceFile, offset + cw->dataRef, SEEK_SET) != 0)
			return 0;
		ret = fread(samples, samplesize, idealSamples, cw->sourceFile);
	}
#if CWAV_ENABLE_ADPCM
	/* the real effort starts with the two ADPCMs */
	else if(cw->encoding == CWAV_ENC_DSP_ADPCM)
	{
		bytesize = align_14_down(bytesize);
		/* needs fixing */
		size_t max_samples = dsp_to_samples(bytesize);
		size_t max_to_read = max_samples > samples_left ? samples_left : max_samples;
		if(!max_to_read) return 0;

		/* we need not worry about the alignment of cw->frameCounters as the internal functions always preserve
		 * correct alignment */
		if(fseek(cw->sourceFile, dsp_to_bytes(cw->frameCounters[channel]), SEEK_SET) != 0)
			return 0;

		size_t to_read = dsp_to_bytes_align(max_to_read);
		if(to_read > bytesize) to_read = bytesize;
		ret = dsp_to_samples(fread(samples, 1, to_read, cw->sourceFile));

		/* due to aligning this may occur */
		ret = ret > max_to_read ? max_to_read : ret;
	}
	/* here we have to actually do the decoding to PCM16 ourselves since ndsp does not support it */
	else if(cw->encoding == CWAV_ENC_IMA_ADPCM)
	{
#define DECBUF_SIZE (0x100)
#define DECBUF_SAMPLES (DECBUF_SIZE * SAMPLES_PER_BYTE)
#define SAMPLES_PER_BYTE 2
		/* Reference:
		 *  https://wiki.multimedia.cx/index.php/IMA_ADPCM */

		static const int ima_step_table[89] = {
			7,     8,     9,     10,    11,    12,    13,    14,    16,    17,
			19,    21,    23,    25,    28,    31,    34,    37,    41,    45,
			50,    55,    60,    66,    73,    80,    88,    97,    107,   118,
			130,   143,   157,   173,   190,   209,   230,   253,   279,   307,
			337,   371,   408,   449,   494,   544,   598,   658,   724,   796,
			876,   963,   1060,  1166,  1282,  1411,  1552,  1707,  1878,  2066,
			2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,
			5894,  6484,  7132,  7845,  8630,  9493,  10442, 11487, 12635, 13899,
			15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
		};

		static const int ima_index_table[16] = {
			-1, -1, -1, -1,  2,  4,  6,  8,
			-1, -1, -1, -1,  2,  4,  6,  8,
		};

		if(fseek(cw->sourceFile, cw->samplesOffsets[channel] + (cw->frameCounters[channel] / SAMPLES_PER_BYTE), SEEK_SET) != 0)
			return 0;

		/* decode as 16-bit samples, thus we may at max decode buffersize/sizeof(int16_t) */
		size_t max_samples = bytesize / 2;
		if(max_samples > samples_left)
			max_samples = samples_left;

		char decoder_buffer[DECBUF_SIZE];
		unsigned new_samples, samples_to_read;
		int16_t diff, predictor, step_index, step, *samples16 = (int16_t *) samples;
		uint8_t nibble;

		predictor = cw->IMA_ADPCM[channel].predictor;
		step_index = cw->IMA_ADPCM[channel].step_index;
		step = ima_step_table[step_index];

		while(ret < max_samples)
		{
			samples_left = max_samples - ret;
			samples_to_read = DECBUF_SAMPLES > samples_left ? samples_left : DECBUF_SAMPLES;
			new_samples = fread(decoder_buffer, 1, samples_to_read / SAMPLES_PER_BYTE, cw->sourceFile) * SAMPLES_PER_BYTE;
			for(size_t j = 0; j < new_samples; ++j, ++ret)
			{
				nibble = (decoder_buffer[j >> 1] >> (4 * (j & 1))) & 0xF;

				step_index = step_index + ima_index_table[nibble];

				/* clamp step_index in 0-88 */
				if(step_index <  0) step_index = 0;
				else if(step_index > 88) step_index = 88;

				               diff  = step / 8;
				if(nibble & 4) diff += step / 1;
				if(nibble & 2) diff += step / 2;
				if(nibble & 1) diff += step / 4;

				/* sign bit */
				if(nibble & 8) predictor -= diff;
				else           predictor += diff;

				samples16[ret] = predictor;
				step = ima_step_table[step_index];
			}
			/* we're at the end before we filled max_samples */
			if(new_samples != DECBUF_SAMPLES)
				break;
		}

		cw->IMA_ADPCM[channel].predictor = predictor;
		cw->IMA_ADPCM[channel].step_index = step_index;
#undef SAMPLES_PER_BYTE
#undef DECBUF_SIZE
#undef DECBUF_SAMPLES
	}
#endif

	cw->frameCounters[channel] += ret;
	return ret;
}

int cwav_can_read(struct cwav *cw)
{
	int ret = 1;
	for(int i = 0; i < cw->nchannels; ++i)
		ret &= cw->frameCounters[i] < cw->endFrame ? 1 : 0;
	return ret;
}

void cwav_to_looppoint(struct cwav *cw)
{
	cw->frameCounters[CWAV_LEFT] = cw->frameCounters[CWAV_RIGHT] = cw->loopPoint;
#if CWAV_ENABLE_ADPCM
	if(cw->encoding == CWAV_ENC_IMA_ADPCM)
		for(int i = 0; i < 2; ++i)
		{
			cw->IMA_ADPCM[i].step_index = cw->IMA_ADPCM[i].step_indexLoop;
			cw->IMA_ADPCM[i].predictor = cw->IMA_ADPCM[i].predictorLoop;
		}
	else if(cw->encoding == CWAV_ENC_DSP_ADPCM)
		for(int i = 0; i < 2; ++i)
			cw->dspAdpcmContext[i] = cw->DSP_ADPCM[i].contextLoop;
#endif
}

void cwav_to_0(struct cwav *cw)
{
	cw->frameCounters[CWAV_LEFT] = cw->frameCounters[CWAV_RIGHT] = 0;
#if CWAV_ENABLE_ADPCM
	if(cw->encoding == CWAV_ENC_IMA_ADPCM)
		for(int i = 0; i < 2; ++i)
		{
			cw->IMA_ADPCM[i].step_index = cw->IMA_ADPCM[i].step_index0;
			cw->IMA_ADPCM[i].predictor = cw->IMA_ADPCM[i].predictor0;
		}
	else if(cw->encoding == CWAV_ENC_DSP_ADPCM)
		for(int i = 0; i < 2; ++i)
			cw->dspAdpcmContext[i] = cw->DSP_ADPCM[i].context0;
#endif
}

