#include <unistd.h>
#include <sys/time.h>
#include <string.h>

#include "QHelper.h"

namespace qedis
{

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

std::size_t BitCount(const uint8_t*  buf, std::size_t len)
{
    std::size_t cnt = 0;
    std::size_t loop = len / 4;
    std::size_t remain = len % 4;

    for (std::size_t i = 0; i  < loop; ++ i)
    {
        cnt += bitsinbyte[buf[4 * i]];
        cnt += bitsinbyte[buf[4 * i + 1]];
        cnt += bitsinbyte[buf[4 * i + 2]];
        cnt += bitsinbyte[buf[4 * i + 3]];
    }

    for (std::size_t i = 0; i  < remain; ++ i)
    {
        cnt += bitsinbyte[buf[4 * loop + i]];
    }

    return cnt;
}
    
/* Copy from redis source.
 * Generate the Redis "Run ID", a SHA1-sized random number that identifies a
 * given execution of Redis, so that if you are talking with an instance
 * having run_id == A, and you reconnect and it has run_id == B, you can be
 * sure that it is either a different instance or it was restarted. */
void getRandomHexChars(char *p, unsigned int len)
{
    FILE *fp = fopen("/dev/urandom","r");
    const char *charset = "0123456789abcdef";
    unsigned int j;
        
    if (fp == NULL || fread(p,len,1,fp) == 0) {
        /* If we can't read from /dev/urandom, do some reasonable effort
         * in order to create some entropy, since this function is used to
         * generate run_id and cluster instance IDs */
        char *x = p;
        unsigned int l = len;
        struct timeval tv;
        pid_t pid = getpid();
            
        /* Use time and PID to fill the initial array. */
        gettimeofday(&tv,NULL);
        if (l >= sizeof(tv.tv_usec)) {
            memcpy(x,&tv.tv_usec,sizeof(tv.tv_usec));
            l -= sizeof(tv.tv_usec);
            x += sizeof(tv.tv_usec);
        }
        if (l >= sizeof(tv.tv_sec)) {
            memcpy(x,&tv.tv_sec,sizeof(tv.tv_sec));
            l -= sizeof(tv.tv_sec);
            x += sizeof(tv.tv_sec);
        }
        if (l >= sizeof(pid)) {
            memcpy(x,&pid,sizeof(pid));
            l -= sizeof(pid);
            x += sizeof(pid);
        }
        /* Finally xor it with rand() output, that was already seeded with
         * time() at startup. */
        for (j = 0; j < len; j++)
            p[j] ^= rand();
    }
    /* Turn it into hex digits taking just 4 bits out of 8 for every byte. */
    for (j = 0; j < len; j++)
        p[j] = charset[p[j] & 0x0F];
    fclose(fp);
}


}
