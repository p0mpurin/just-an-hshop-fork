
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

typedef uint64_t u64;
typedef uint32_t u32;

int make_hwav(const char *output, const char *input, int tagc, char *tags[]);
int make_hstx(const char *output, const char *cfgfile);

static int maketheme(int argc, char *argv[])
{
	if(argc < 3)
	{
		fprintf(stderr, "Usage: maketheme [input-file] [output-file]\n");
		return 1;
	}
	return make_hstx(argv[2], argv[1]);
}

static int makehwav(int argc, char *argv[])
{
	if(argc < 3)
	{
		fprintf(stderr, "Usage: makehwav [input-file] [output-file] [tag-name=tag-value | - (remove all tags)]...\n");
		return 1;
	}
	return make_hwav(argv[2], argv[1], argc - 3, &argv[3]);
}

int main(int argc, char *argv[])
{
	if(argc < 2)
	{
error:
		fprintf(stderr, "Usage: %s [maketheme | makehwav]\n", argv[0]);
		return 1;
	}
	if(strcmp(argv[1], "maketheme") == 0)
		return maketheme(argc - 1, &argv[1]);
	if(strcmp(argv[1], "makehwav") == 0)
		return makehwav(argc - 1, &argv[1]);
	goto error;
}

