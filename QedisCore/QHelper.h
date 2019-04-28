#ifndef BERT_HELPER_H
#define BERT_HELPER_H

#include "QString.h"
#include <vector>
#include <cstdlib>

namespace qedis
{

// hash func from redis
extern unsigned int dictGenHashFunction(const void* key, int len);

// hash function
struct Hash
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

    size_t lucky = random() % container.size();
    for (size_t i = 0; i < container.bucket_count(); ++i)
    {
        if (lucky >= container.bucket_size(i))
        {
            lucky -= container.bucket_size(i);
        }
        else
        {
            auto it = container.begin(i);
            while (lucky > 0)
            {
                ++ it;
                -- lucky;
            }

            return it;
        }
    }

    // never here
    return typename HASH::const_local_iterator();
}

// scan
template <typename HASH>
inline size_t ScanHashMember(const HASH& container,
                             size_t cursor,
                             size_t count,
                             std::vector<typename HASH::const_local_iterator>& res)
{
    if (cursor >= container.size())
    {
        return 0;
    }

    auto idx = cursor;
    for (decltype(container.bucket_count()) bucket = 0;
         bucket < container.bucket_count();
         ++ bucket)
    {
        const auto bktSize = container.bucket_size(bucket);
        if (idx < bktSize)
        {
            // find the bucket;
            auto it = container.begin(bucket);
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

            return newCursor;
        }
        else
        {
            idx -= bktSize;
        }
    }

    return 0; // never here
}
    
extern void getRandomHexChars(char *p, unsigned int len);

enum MemoryInfoType {
    VmPeak = 0,
    VmSize = 1,
    VmLck = 2,
    VmHWM = 3,
    VmRSS = 4,
    VmSwap = 5,

    VmMax = VmSwap + 1,
};

extern std::vector<size_t> getMemoryInfo();
extern size_t getMemoryInfo(MemoryInfoType type);
    
}

#endif

