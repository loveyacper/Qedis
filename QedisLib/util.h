#ifndef BERT_UTIL_H
#define BERT_UTIL_H

#include "QString.h"
#include <vector> 
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
        size_t bucket = random() % container.bucket_count();
        if (container.bucket_size(bucket) == 0)
            continue;
        
        long lucky = random() % container.bucket_size(bucket);
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

// scan
template <typename HASH>
inline size_t ScanHashMember(const HASH& container, size_t cursor, size_t count, std::vector<typename HASH::const_local_iterator>& res)
{
    if (cursor >= container.size())
    {
        return 0;
    }

    auto  idx = cursor;
    for (auto bucket = 0; bucket < container.bucket_count(); ++ bucket)
    {
        const auto bktSize = container.bucket_size(bucket);
        if (idx < bktSize)
        {
            // find the bucket;
            auto   it = container.begin(bucket);
            while (idx > 0)
            {
                ++ it;
                -- idx;
            }

            size_t newCursor = cursor;
            auto   end = container.end(bucket);
            while (res.size() < count && it != end)
            {
                ++ newCursor;
                res.push_back(it ++);

                if (it == end)
                {
                    while (++ bucket < container.bucket_count())
                    {
                        if (container.bucket_size(bucket) > 0)
                        {
                            it  = container.begin(bucket);
                            end = container.end(bucket);
                            break;
                        }
                    }

                    if (bucket == container.bucket_count())
                        return  0;
                }
            }

            return  newCursor;
        }
        else
        {
            idx -= bktSize;
        }
    }

    return 0;   // never here
}

#endif

