#ifndef inc_lzss_hh
#define inc_lzss_hh

#include <3ds.h>
#include <cstdlib>
#include <cstring>

namespace lzss
{
    u8 *decompress(u8 *orig, size_t siz, size_t *nsiz);
};

#endif