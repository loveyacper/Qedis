#ifndef BERT_UTIL_H
#define BERT_UTIL_H

#include "QString.h"

          
//void dictSetHashFunctionSeed(uint32_t seed) ; 
//unsigned int dictGetHashFunctionSeed(void) ; 

// hash func from redis
extern unsigned int dictGenHashFunction(const void* key, int len);

// hash function
struct my_hash
{
    size_t operator() (const QString& str) const;
};

std::size_t BitCount(const uint8_t*  buf, std::size_t len);

#endif

