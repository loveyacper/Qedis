#include "util.h"

using namespace std;

static unsigned dict_hash_function_seed = 5381;

// hash func from redis
unsigned int dictGenHashFunction(const void* key, int len) {
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    unsigned seed = dict_hash_function_seed;
    const unsigned m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    unsigned h = seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const unsigned char *data = (const unsigned char *)key;

    while(len >= 4) {
        unsigned int k = *(unsigned int*)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch(len) {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0]; h *= m;
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

// hash function
size_t my_hash::operator() (const QString& str) const {
    return dictGenHashFunction(str.data(), static_cast<int>(str.size()));
}


static const uint8_t bitsinbyte[256] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,1,2,2,3,2,3,3,4,2,3,3,4,3,4, 4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4, 5,4, 5,5,6,4,5,5,6,5,6,6,7,1,2,2,3,2,3,3,4,2,3,3,4,3,4,4,5,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5, 5,6,4,5,5,6, 5,6,6,7,2,3,3,4,3,4,4,5,3,4,4,5,4,5,5,6,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,3,4,4,5,4,5,5,6,4,5,5,6,5,6,6,7,4,5,5,6,5,6,6,7,5, 6,6,7,6,7,7,8};

int BitCount(const uint8_t*  buf, int len)
{
    int cnt = 0;
    int loop = len / 4;
    int remain = len % 4;

    for (int i = 0; i  < loop; ++ i)
    {
        cnt += bitsinbyte[buf[4 * i]];
        cnt += bitsinbyte[buf[4 * i + 1]];
        cnt += bitsinbyte[buf[4 * i + 2]];
        cnt += bitsinbyte[buf[4 * i + 3]];
    }

    for (int i = 0; i  < remain; ++ i)
    {
        cnt += bitsinbyte[buf[4 * loop + i]];
    }

    return cnt;
}

