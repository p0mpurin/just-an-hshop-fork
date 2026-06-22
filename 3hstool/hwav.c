
/* HWAV encoder using ffmpeg
 */

#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static uint16_t u16;
static uint32_t u32;

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	#define U16(i) ((u16 = __builtin_bswap16(i)), (unsigned char *) &u16)
	#define U32(i) ((u32 = __builtin_bswap32(i)), (unsigned char *) &u32)
#elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	#define U16(i) ((u16 = i), (uint16_t *) &u16)
	#define U32(i) ((u32 = i), (uint32_t *) &u32)
#else
	#error "Unsupported endian"
#endif

#define flt2s16(flt) ((int16_t) ((flt) * ((float) INT16_MAX)))
/* maybe in the future: encoding s32/s64, just divide by INTxx_MAX and then multiply by INT16_MAX */

enum HWAV_ReferenceType {
	HWAV_SampleData    = 0x1F00,
	HWAV_InfoBlock     = 0x7000,
	HWAV_DataBlock     = 0x7001,
	HWAV_ChannelInfo   = 0x7100,
	/* extended blocks */
	HWAV_VorbisComment = 0x8000,
};

struct CWAV_sized_reference {
	uint16_t typeId;
	uint16_t padding2;
	uint32_t offset;
	uint32_t size;
} __attribute__((packed));

struct CWAV_reference {
	uint16_t typeId;
	uint16_t padding1;
	uint32_t offset;
} __attribute__((packed));

struct HWAV_header {
	char magic[4]; /* "HWAV" */
	uint16_t extendedBlockCount;
	struct CWAV_sized_reference extendedBlocks[];
} __attribute__((packed));

struct CWAV_info_block {
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
	struct CWAV_reference refs[]; /* to channel info objects */
} __attribute__((packed));

struct CWAV_header {
	char magic[4]; /* "CWAV" */
	uint16_t endian;
	uint16_t headerSize;
	uint32_t version;
	uint32_t fileSize;
	uint16_t nblocks;
	uint16_t reserved1;
	struct CWAV_sized_reference infoRef;
	struct CWAV_sized_reference dataRef;
	char paddingA[0x40 - 0x2C];
} __attribute__((packed));

struct CWAV_channel_info {
	struct CWAV_reference samplesRef;
	struct CWAV_reference adpcmInfoRef; /* either cwav_dsp_adpcm_info or cwav_ima_adpcm_info */
	uint32_t reserved3;
} __attribute__((packed));

struct CWAV_data_block {
	char magic[4]; /* "DATA" */
	uint32_t size;
	unsigned char samples[];
};

struct HWAV_VorbisComment_Block {
	char magic[4]; /* "VCOM" */
	uint32_t size;
	unsigned char vorbis_comment_data[];
};


static const unsigned char HWAV_vendorstring[] = "HWAV Reference C Encoder v1.0";

#define TRYWRITE(buf, size) if(fwrite(buf, size, 1, hwav) != 1) { fprintf(stderr, "Failed to write %zu bytes.\n", size); goto fail; }

int make_hwav(const char *output, const char *input, int tagc, char *tags[])
{
	avformat_network_init();

	AVIOContext *io_vorbiscomment = NULL;
	AVDictionaryEntry *dicte = NULL;
	struct SwrContext *swr_ctx = NULL;
	uint8_t *vorbiscomment = NULL;
	AVFormatContext *avfc = NULL;
	AVDictionary *ftags = NULL;
	const AVCodec *acodec = NULL;
	AVCodecContext *acodec_ctx = NULL;
	AVPacket *pkt = NULL;
	AVFrame *ifrm = NULL;
	AVFrame *ofrm = NULL;
	AVFrame *frm;
	AVStream *astream;
	int ret = 0, astream_nb;
	FILE *hwav = NULL;
	char *tagsbuf;

	int res = avformat_open_input(&avfc, input, NULL, NULL);
	if(res < 0)
	{
		fprintf(stderr, "Failed to open %s: %s.\n", input, av_err2str(res));
		return 1;
	}

	res = avformat_find_stream_info(avfc, NULL);
	if(res < 0)
	{
		fprintf(stderr, "Failed to find stream info: %s.\n", av_err2str(res));
		goto fail;
	}

	printf("    == Input file for HWAV generation ==\n");
	av_dump_format(avfc, 0, input, 0);

	astream_nb = av_find_best_stream(avfc, AVMEDIA_TYPE_AUDIO, -1, -1, &acodec, 0);
	if(astream_nb < 0)
	{
		fprintf(stderr, "Failed to find the best audio stream: %s.\n", av_err2str(astream_nb));
		goto fail;
	}
	astream = avfc->streams[astream_nb];

	printf("Stream idx: %i, acodec: %s (%s)\n", res, acodec->long_name, acodec->name);

	pkt = av_packet_alloc();
	if(!pkt)
	{
		fprintf(stderr, "Failed to allocate an AVPacket.\n");
		goto fail;
	}

	ifrm = av_frame_alloc();
	ofrm = av_frame_alloc();
	if(!ifrm || !ofrm)
	{
		fprintf(stderr, "Failed to allocate AVFrames.\n");
		goto fail;
	}

	acodec_ctx = avcodec_alloc_context3(acodec);
	if(!acodec_ctx)
	{
		fprintf(stderr, "Failed to allocate an AVCodecContext.\n");
		goto fail;
	}

	res = avcodec_parameters_to_context(acodec_ctx, astream->codecpar);
	if(res < 0)
	{
		fprintf(stderr, "Failed to convert parameters to context: %s.\n", av_err2str(res));
		goto fail;
	}

	res = avcodec_open2(acodec_ctx, acodec, NULL);
	if(res < 0)
	{
		fprintf(stderr, "Failed to open audio context: %s.\n", av_err2str(res));
		goto fail;
	}

	av_dict_copy(&ftags, avfc->metadata, 0);
	av_dict_copy(&ftags, astream->metadata, 0);
	for(int i = 0; i < tagc; ++i)
	{
		/* a "-" is "clear current tags" */
		if(strcmp(tags[i], "-") == 0)
		{
			av_dict_free(&ftags);
			ftags = NULL;
			continue;
		}
		char *eq = strchr(tags[i], '=');
		if(!eq)
		{
			fprintf(stderr, "Warning: unsetting tag \"%s\" (if it exists--case insensitive). Was this really what you intended?\n", tags[i]);
			av_dict_set(&ftags, tags[i], NULL, 0);
		}
		else
		{
			*eq = '\0';
			av_dict_set(&ftags, tags[i], eq + 1, 0);
		}
	}
	av_dict_get_string(ftags, &tagsbuf, '=', ',');
	printf("Inserting these tags into the HWAV: %s.\n", *tagsbuf ? tagsbuf : "(none)");
	av_free(tagsbuf);

	res = avio_open_dyn_buf(&io_vorbiscomment);
	if(res < 0)
	{
		fprintf(stderr, "Failed to open dynamic vorbis comment buffer: %s.\n", av_err2str(res));
		goto fail;
	}

	/* encode vorbis comment */
	avio_write(io_vorbiscomment, (unsigned char *) U32(sizeof(HWAV_vendorstring) - 1), 4);
	avio_write(io_vorbiscomment, HWAV_vendorstring, sizeof(HWAV_vendorstring) - 1);
	avio_write(io_vorbiscomment, (unsigned char *) U32(av_dict_count(ftags)), 4);
	while((dicte = av_dict_get(ftags, "", dicte, AV_DICT_IGNORE_SUFFIX)))
	{
		size_t size_key = strlen(dicte->key), size_value = strlen(dicte->value);
		avio_write(io_vorbiscomment, (unsigned char *) U32(size_key + size_value + 1), 4);
		avio_write(io_vorbiscomment, (unsigned char *) dicte->key, size_key);
		static const unsigned char equal = '=';
		avio_write(io_vorbiscomment, &equal, 1);
		avio_write(io_vorbiscomment, (unsigned char *) dicte->value, size_value);
	}
	size_t realvorbiscommentsize = avio_close_dyn_buf(io_vorbiscomment, &vorbiscomment);

	int byps = av_get_bytes_per_sample(acodec_ctx->sample_fmt);

	if(acodec_ctx->sample_fmt != AV_SAMPLE_FMT_U8P && acodec_ctx->sample_fmt != AV_SAMPLE_FMT_S16P
		&& acodec_ctx->sample_fmt != AV_SAMPLE_FMT_U8 && acodec_ctx->sample_fmt != AV_SAMPLE_FMT_S16)
	{
		if(acodec_ctx->sample_fmt == AV_SAMPLE_FMT_FLTP || acodec_ctx->sample_fmt == AV_SAMPLE_FMT_FLT)
			byps = 2; /* we'll convert to s16 */
		else
		{
			fprintf(stderr, "Unsupported sample format! %s.\n", av_get_sample_fmt_name(acodec_ctx->sample_fmt));
			goto fail;
		}
#if 0
		fprintf(stderr, "Warning: this file requires resampling, from %s to PCM16\n", av_get_sample_fmt_name(acodec_ctx->sample_fmt));
		swr_ctx = swr_alloc_set_opts(
			NULL,
			acodec_ctx->channel_layout,
			RESAMPLE_FMT,
			acodec_ctx->sample_rate,
			acodec_ctx->channel_layout,
			acodec_ctx->sample_fmt,
			acodec_ctx->sample_rate,
			0, NULL
		);

		res = swr_init(swr_ctx);
		if(res < 0)
		{
			fprintf(stderr, "Error! failed to initialize resampler: %s.\n", av_err2str(res));
			goto fail;
		}

		byps = 2;
#endif
	}

	int channels = acodec_ctx->ch_layout.nb_channels;
	if(channels != 1 && channels != 2)
	{
		fprintf(stderr, "At max 2 channels are supported. The file has %i channels.\n", channels);
		goto fail;
	}

	printf("Sample rate: %iHz\n", acodec_ctx->sample_rate);

	size_t hwavHeaderSize = 4 + 2 + (12*1);
	size_t vorbiscommentRef = 0x40 + hwavHeaderSize;
	size_t vorbiscommentSize = realvorbiscommentsize + 8;
	size_t inforef = vorbiscommentRef + vorbiscommentSize;
	size_t inforefSize = 0x20 + ((8+0x14) * channels);
	size_t dataref = inforef + inforefSize;

	hwav = fopen(output, "w");
	if(!hwav)
	{
		fprintf(stderr, "Failed to open output (%s): %s.\n", output, strerror(errno));
		goto fail;
	}

	struct CWAV_header chdr;
	memset(&chdr, 0x00, sizeof(chdr));
	TRYWRITE(&chdr, sizeof(chdr));

	struct HWAV_header hhdr;
	memcpy(hhdr.magic, "HWAV", 4);
	hhdr.extendedBlockCount = *U16(1);
	struct CWAV_sized_reference vref;
	vref.typeId = *U16(HWAV_VorbisComment);
	vref.padding2 = 0;
	vref.offset = *U32(vorbiscommentRef);
	vref.size = *U32(vorbiscommentSize);

	TRYWRITE(&hhdr, sizeof(hhdr));
	TRYWRITE(&vref, sizeof(vref));

	struct HWAV_VorbisComment_Block vcom;
	memcpy(vcom.magic, "VCOM", 4);
	vcom.size = *U32(vorbiscommentSize);
	TRYWRITE(&vcom, sizeof(vcom));
	TRYWRITE(vorbiscomment, realvorbiscommentsize);

	size_t info_block_offset = ftell(hwav);
	struct CWAV_info_block info;
	memcpy(info.magic, "INFO", 4);
	info.size = *U32(inforefSize);
	info.encoding = byps - 1;
	info.loop = 0;
	info.padding3 = 0;
	info.sampleRate = *U32(acodec_ctx->sample_rate);
	info.loopStartFrame = 0;
	info.loopEndFrame = 0; /* will be filled at the end */
	info.reserved2 = 0;
	info.nrefs = *U32(channels);

	TRYWRITE(&info, sizeof(info));
	struct CWAV_reference ref;
	for(int i = 0; i < channels; ++i)
	{
		ref.typeId = *U16(HWAV_ChannelInfo);
		ref.padding1 = 0;
		ref.offset = 4 + (8 * channels) + (0x14 * i);
		TRYWRITE(&ref, sizeof(ref));
	}
	struct CWAV_channel_info cinf;
	size_t cinf_offset;
	for(int i = 0; i < channels; ++i)
	{
		cinf_offset = ftell(hwav);
		cinf.samplesRef.typeId = *U16(HWAV_SampleData);
		cinf.samplesRef.padding1 = 0;
		cinf.samplesRef.offset = *U32(8);
		cinf.adpcmInfoRef.typeId = *U16(0x300);
		cinf.adpcmInfoRef.padding1 = 0;
		cinf.adpcmInfoRef.offset = *U32(0xFFFFFFFF);
		cinf.reserved3 = 0;
		TRYWRITE(&cinf, sizeof(cinf));
	}
	/* data ... */
	size_t sample_block_offset = ftell(hwav);
	struct CWAV_data_block dblock;
	memcpy(dblock.magic, "DATA", 4);
	dblock.size = 0;
	TRYWRITE(&dblock, sizeof(dblock));

	size_t sample_count;

	if(av_sample_fmt_is_planar(acodec_ctx->sample_fmt))
		printf("data is planar\n");
	else
		printf("data is not planar\n");

	int channel_index = 0;
	unsigned char *sample_buf = NULL;
	while(1)
	{
		sample_count = 0;
		while(av_read_frame(avfc, pkt) >= 0)
		{
			res = avcodec_send_packet(acodec_ctx, pkt);
			if(res < 0)
			{
				fprintf(stderr, "Failed to send packet to decoder: %s.\n", av_err2str(res));
				goto fail;
			}

			res = avcodec_receive_frame(acodec_ctx, ifrm);
			if(res < 0)
			{
				fprintf(stderr, "Failed to receive frame from decoder: %s.\n", av_err2str(res));
				goto fail;
			}

			frm = ifrm;

			if(frm->sample_rate != acodec_ctx->sample_rate)
			{
				fprintf(stderr, "Error! variable sample rate not supported.\n");
				goto fail;
			}

			if(frm->flags & AV_FRAME_FLAG_CORRUPT)
			{
				fprintf(stderr, "Warning: Corrupt frame.\n");
				goto next;
			}

			if(frm->flags & AV_FRAME_FLAG_DISCARD)
			{
				fprintf(stderr, "Warning: discarding frame.\n");
				goto next;
			}

			if(frm->format != acodec_ctx->sample_fmt)
			{
				fprintf(stderr, "Error! variable sample format!\n");
				goto fail;
			}

#if 0
			if(swr_ctx)
			{
				av_samples_alloc(&samples, NULL, ifrm->nb_channels, ifrm->nb_samples, RESAMPLE_FMT, 1);
				swr_convert();
#if 0
				/* we need to resample... */
				ofrm->channel_layout = ifrm->channel_layout;
				ofrm->sample_rate = ifrm->sample_rate;
				ofrm->format = RESAMPLE_FMT;
				res = swr_convert_frame(swr_ctx, ofrm, ifrm);
				if(res < 0)
				{
					fprintf(stderr, "Failed to resample: %s.\n", av_err2str(res));
					goto fail;
				}
				frm = ofrm;
#endif
			}
#endif

			if(frm->format == AV_SAMPLE_FMT_S16P)
			{
				size_t bytes = byps * frm->nb_samples;
				TRYWRITE(frm->data[channel_index], bytes)
			}
			else if(frm->format == AV_SAMPLE_FMT_S16 || swr_ctx)
			{
				unsigned char *buffer = frm->data[0];
				if(swr_ctx) buffer = sample_buf;
				for(int j = 0, offset = channel_index * byps; j < frm->nb_samples; ++j, offset += channels * byps)
					TRYWRITE(buffer + offset, (size_t) 2);
			}
			else if(frm->format == AV_SAMPLE_FMT_U8)
			{
				signed char sample;
				for(int j = 0, offset = channel_index * byps; j < frm->nb_samples; ++j, offset += channels * byps)
				{
					sample = (*((unsigned char *) frm->data[0] + offset)) - 128;
					TRYWRITE(&sample, (size_t) 1);
				}
			}
			else if(frm->format == AV_SAMPLE_FMT_U8P)
			{
				signed char sample;
				for(int i = 0; i < frm->nb_samples; ++i)
				{
					sample = (*((unsigned char *) frm->data[channel_index] + i)) - 128;
					TRYWRITE(&sample, (size_t) 1);
				}
			}
			else if(frm->format == AV_SAMPLE_FMT_FLTP)
			{
				int16_t sample;
				float *fa = (float *) frm->data[channel_index];
				for(int i = 0; i < frm->nb_samples; ++i)
				{
					sample = flt2s16(fa[i]);
					TRYWRITE(&sample, sizeof(sample));
				}
			}
			else if(frm->format == AV_SAMPLE_FMT_FLT)
			{
				int16_t sample;
				float *fa = (float *) frm->data[0];
				for(int i = 0; i < frm->nb_samples; ++i)
				{
					sample = flt2s16(fa[i * channels + channel_index]);
					TRYWRITE(&sample, sizeof(sample));
				}
			}

			sample_count += frm->nb_samples;

	next:
			av_packet_unref(pkt);
		}
		++channel_index;
		if(channel_index == channels)
			break;
		av_seek_frame(avfc, astream_nb, 0, AVSEEK_FLAG_BACKWARD);
		avformat_flush(avfc);
	}
	av_free(sample_buf);

	/* now we fix a few things:
	 *  - header
	 *  - info sample count
	 *  - sample block size
	 *  - sample block size in header
	 *  - maybe second channel info sample ref */

	size_t sampleBlockSize = byps * sample_count;
	size_t datarefSize = 8 + (sampleBlockSize * channels);
	size_t computedFileSize = dataref + datarefSize;

	printf("sampleBlockSize = %zu, channels = %i\n", sampleBlockSize, channels);
	printf("computed file size is: %zu\n", computedFileSize);

	size_t fileSize = ftell(hwav);
	if(fileSize != computedFileSize)
	{
		fprintf(stderr, "Internal error! computed file size is not actual file size (%zu)!\n", fileSize);
		goto fail;
	}

	fseek(hwav, 0, SEEK_SET);
	memcpy(chdr.magic, "CWAV", 4);
	chdr.endian = *U16(0xFEFF);
	chdr.headerSize = *U16(0x40);
	chdr.version = *U32(0x02010000);
	chdr.fileSize = fileSize;
	chdr.nblocks = *U16(2);
	chdr.reserved1 = 0;
	chdr.infoRef.typeId = *U16(HWAV_InfoBlock);
	chdr.infoRef.padding2 = 0;
	chdr.infoRef.offset = *U32(inforef);
	chdr.infoRef.size = *U32(inforefSize);
	chdr.dataRef.typeId = *U16(HWAV_DataBlock);
	chdr.dataRef.padding2 = 0;
	chdr.dataRef.offset = *U32(dataref);
	chdr.dataRef.size = *U32(datarefSize);
	TRYWRITE(&chdr, sizeof(chdr));

	fseek(hwav, info_block_offset, SEEK_SET);
	info.loopEndFrame = *U32(sample_count);
	TRYWRITE(&info, sizeof(info));

	fseek(hwav, sample_block_offset, SEEK_SET);
	dblock.size = *U32(datarefSize);
	TRYWRITE(&dblock, sizeof(dblock));

	if(channels == 2)
	{
		fseek(hwav, cinf_offset, SEEK_SET);
		cinf.samplesRef.offset = *U32(8 + sampleBlockSize);
		TRYWRITE(&cinf, sizeof(cinf));
	}

exit:
	avformat_close_input(&avfc);
	avformat_network_deinit();
	av_packet_free(&pkt);
	av_dict_free(&ftags);
	av_free(vorbiscomment);
	swr_free(&swr_ctx);
	av_frame_free(&ofrm);
	av_frame_free(&ifrm);
	if(hwav)
		fclose(hwav);
	return ret;
fail:
	unlink(output);
	ret = 1;
	goto exit;
}

