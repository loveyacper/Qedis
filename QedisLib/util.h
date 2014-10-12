#ifndef BERT_UTIL_H
#define BERT_UTIL_H

#include "QString.h"
#include <cstdlib> 

// hash func from redis
extern unsigned int dictGenHashFunction(const void* key, int len);

// hash function
struct my_hash
{
    size_t operator() (const QString& str) const;
};

std::size_t BitCount(const uint8_t*  buf, std::size_t len);

template <typename HASH>
inline typename HASH::const_local_iterator RandomHashMember(const HASH& container)
{
    if (container.empty())
    {
        return typename HASH::const_local_iterator();
    }
    
    while (true)
    {
        size_t bucket = rand() % container.bucket_count();
        if (container.bucket_size(bucket) == 0)
            continue;
        
        int lucky = rand() % container.bucket_size(bucket);
        typename HASH::const_local_iterator it = container.begin(bucket);
        while (lucky > 0)
        {
            ++ it;
            -- lucky;
        }
        
        return it;
    }
    
    
    return typename HASH::const_local_iterator();
}

#endif

