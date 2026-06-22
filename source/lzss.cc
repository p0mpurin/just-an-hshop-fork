#include "lzss.hh"

static int lzss_decompress_buffer(u8* compressed, u32 compressedsize, u8* decompressed, u32 decompressedsize)
{
	u8* footer = compressed + compressedsize - 8;
	u32 buffertopandbottom = * (uint32_t *) (footer+0);
	//u32 originalbottom = getle32(footer+4);
	u32 i, j;
	u32 out = decompressedsize;
	u32 index = compressedsize - ((buffertopandbottom>>24)&0xFF);
	u32 segmentoffset;
	u32 segmentsize;
	u8 control;
	u32 stopindex = compressedsize - (buffertopandbottom&0xFFFFFF);

	memset(decompressed, 0, decompressedsize);
	memcpy(decompressed, compressed, compressedsize);

	while(index > stopindex)
	{
		control = compressed[--index];

		for(i=0; i<8; i++)
		{
			if (index <= stopindex)
				break;

			if (index <= 0)
				break;

			if (out <= 0)
				break;

			if (control & 0x80)
			{
				if (index < 2)
					goto clean;

				index -= 2;

				segmentoffset = compressed[index] | (compressed[index+1]<<8);
				segmentsize = ((segmentoffset >> 12)&15)+3;
				segmentoffset &= 0x0FFF;
				segmentoffset += 2;

				if (out < segmentsize)
					goto clean;

				for(j=0; j<segmentsize; j++)
				{
					u8 data;

					if (out+segmentoffset >= decompressedsize)
						goto clean;

					data  = decompressed[out+segmentoffset];
					decompressed[--out] = data;
				}
			}
			else
			{
				if (out < 1)
					goto clean;

				decompressed[--out] = compressed[--index];
			}

			control <<= 1;
		}
	}

	return 0;

clean:
	return -1;
}

static int lzss_decompress_buffer(u8* compressed, u32 compressedsize, u8* decompressed, u32 decompressedsize);

u8 *lzss::decompress(u8 *orig, size_t siz, size_t *nsiz)
{
	size_t dsize = siz + * (u32 *) (&orig[siz - 4]);

	u8 *ret = new u8[dsize];
	if(!ret) return NULL;

	if(lzss_decompress_buffer(orig, siz, ret, dsize) != 0)
	{
		delete [] ret;
		return NULL;
	}

	*nsiz = dsize;
	return ret;
}