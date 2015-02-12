#ifndef BERT_QVARINT_H
#define BERT_QVARINT_H

#include <vector>
#include <stdint.h>

namespace QVarInt
{

static const uint8_t  kMaxByteValue = 0x7F;
static const uint32_t kMaxBytes = 10;

size_t      Set(uint64_t v, int8_t* buf);
uint64_t    Get(const int8_t* buf);
size_t      GetVarSize(int8_t* buf);

uint64_t    ZigZagEncode64(int64_t v);
int64_t     ZigZagDecode64(uint64_t v);

}

#endif

