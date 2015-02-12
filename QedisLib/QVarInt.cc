#include <iostream>
#include <assert.h>

#include "QVarInt.h"


namespace QVarInt
{

uint64_t   ZigZagEncode64(int64_t v)
{
    return (v << 1) ^ (v >> 63);
}

int64_t    ZigZagDecode64(uint64_t v)
{
    return  (v >> 1) ^ (-static_cast<int64_t>(v & 1));
}


size_t      Set(uint64_t  v, int8_t* buf)
{
    v = ZigZagEncode64(static_cast<int64_t>(v));

    size_t  size = 0;

    while (v > kMaxByteValue)
    {
        buf[size]  = v & kMaxByteValue;
        buf[size] |= 0x80;
        ++ size;

        v >>= 7;
    }

    buf[size ++] = v & kMaxByteValue;
    assert (size <= kMaxBytes);

    return size;
}

uint64_t    Get(const int8_t* buf)
{
    assert(buf);

    uint64_t v = 0;

    for (size_t i = 0; true; ++ i)
    {
        v |= static_cast<uint64_t>(buf[i] & kMaxByteValue) << (7 * i);

        if (!(buf[i] & 0x80))
            break;
    }

    return  ZigZagDecode64(v);
}
    
    
size_t      GetVarSize(int8_t* buf)
{
    size_t  n = 0;
    
    for (size_t i = 0; true; ++ i)
    {
        ++ n;
        
        if (!(buf[i] & 0x80))
            break;
    }
    
    return  n;
}

}

#if 0

using namespace std;
int main(int ac, char* av[])
{
    uint64_t arg = 0;
    if (ac > 1)
        arg = strtol(av[1], NULL, 10);

    int8_t  buf[QVarInt::kMaxBytes];
    size_t  bytes = QVarInt::Set(arg,  buf);
    cout << "bytes = " << bytes << endl;

    uint64_t val = QVarInt::Get(buf, bytes);
 
    cout << val << endl;
    cout << static_cast<int64_t>(val) << endl;
}

#endif

