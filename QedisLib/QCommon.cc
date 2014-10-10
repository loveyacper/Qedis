#include "QCommon.h"
#include "UnboundedBuffer.h"
#include <limits>
#include <cstdlib>
#include <algorithm>


using  std::size_t;

#define CRLF "\r\n"

struct QErrorInfo  g_errorInfo[] = {
    {sizeof "-Fine"- 1,   "-Fine"},
    {sizeof "-ERR Operation against a key holding the wrong kind of value\r\n"- 1, "-ERR Operation against a key holding the wrong kind of value\r\n"}, 
    {sizeof "already exist"- 1,   "already exist"},
    {sizeof "-ERR no such key\r\n" - 1, "-ERR no such key\r\n"},
    {sizeof "-ERR wrong number of arguments\r\n"- 1, "-ERR wrong number of arguments\r\n"},
    {sizeof "-ERR Unknown command\r\n"- 1,   "-ERR Unknown command\r\n"},
    {sizeof "-ERR value is not an integer or out of range\r\n"- 1, "-ERR value is not an integer or out of range\r\n"},
    {sizeof "-ERR syntax error\r\n"-1, "-ERR syntax error\r\n"},
};

int Int2Str(char* ptr, size_t nBytes, long val)
{
    return snprintf(ptr, nBytes - 1, "%ld",  val);
}

bool Str2Long(const char* ptr, size_t nBytes, long& val)
{
    bool negtive = false;
    size_t  i = 0;

    if (ptr[0] == '-' || ptr[0] == '+')
    {
        if (nBytes <= 1 || !isdigit(ptr[1]))
            return false;

        negtive = (ptr[0] == '-');
        i = 1;
    }

    val = 0;
    for (; i < nBytes; ++ i)
    {
        if (!isdigit(ptr[i]))
            break;

        val *= 10;
        val += ptr[i] - '0';
    }

    if (negtive)
    {
        val *= -1;
    }

    return true;
}

bool Strtol(const char* ptr, size_t nBytes, long* outVal)
{
    if (nBytes == 0)
        return false;

    char* pEnd = 0;
    long ret = strtol(ptr, &pEnd, 0);

    // TODO : bug fix
    if (ret > std::numeric_limits<long>::max() ||
        ret < std::numeric_limits<long>::min())
        return false;

    //if (ret > INT32_MAX || ret < INT32_MIN)  return false;
    *outVal = ret;
    return pEnd == ptr + nBytes;
}

bool Strtof(const char* ptr, size_t nBytes, float* outVal)
{
    if (nBytes == 0)
        return false;

    char* pEnd = 0;
    *outVal = strtof(ptr, &pEnd);
    return pEnd == ptr + nBytes;
}


const char* Strstr(const char* ptr, size_t nBytes, const char* pattern, size_t nBytes2)
{
    if (!pattern || *pattern == 0)
        return 0;
    
    const char* ret = std::search(ptr, ptr + nBytes, pattern, pattern + nBytes2);
    return  ret == ptr + nBytes ? 0 : ret;
}

const char* SearchCRLF(const char* ptr, size_t nBytes)
{
    return  Strstr(ptr, nBytes, CRLF, 2);
}

size_t  FormatInt(long value, UnboundedBuffer& reply)
{
    char val[32];
    int len = snprintf(val, sizeof val, "%ld" CRLF, value);
    
    size_t  oldSize = reply.ReadableSize();
    reply.PushData(":", 1);
    reply.PushData(val, len);
    
    return reply.ReadableSize() - oldSize;
}

size_t  FormatSingle(const char* str, size_t len, UnboundedBuffer& reply)
{
    size_t   oldSize = reply.ReadableSize();
    reply.PushData("+", 1);
    reply.PushData(str, len);
    reply.PushData(CRLF, 2);

    return reply.ReadableSize() - oldSize;
}


size_t  FormatBulk(const char* str, size_t len, UnboundedBuffer& reply)
{
    size_t oldSize = reply.ReadableSize();
    reply.PushData("$", 1);

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%lu" CRLF, len);
    reply.PushData(val, tmp);

    if (str && len > 0)
    {
        reply.PushData(str, len);
    }
    
    reply.PushData(CRLF, 2);
    
    return reply.ReadableSize() - oldSize;
}

size_t  PreFormatMultiBulk(size_t nBulk, UnboundedBuffer& reply)
{
    size_t  oldSize = reply.ReadableSize();
    reply.PushData("*", 1);

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%lu" CRLF, nBulk);
    reply.PushData(val, tmp);

    return reply.ReadableSize() - oldSize;
}

void  ReplyError(QError err, UnboundedBuffer& reply)
{
    const QErrorInfo& info = g_errorInfo[err];

    reply.PushData(info.errorStr, info.len);
}

size_t  FormatNull(UnboundedBuffer& reply)
{
    size_t   oldSize = reply.ReadableSize();
    reply.PushData("$-1" CRLF, 5);
    
    return reply.ReadableSize() - oldSize;
}


size_t  FormatNullArray(UnboundedBuffer& reply)
{
    size_t   oldSize = reply.ReadableSize();
    reply.PushData("*-1" CRLF, 5);
    
    return reply.ReadableSize() - oldSize;
}

size_t  FormatOK(UnboundedBuffer& reply)
{
    size_t   oldSize = reply.ReadableSize();
    reply.PushData("+OK" CRLF, 5);
    
    return reply.ReadableSize() - oldSize;
}

size_t Format1(UnboundedBuffer& reply)
{
    const char* val = ":1\r\n";
    
    size_t  oldSize = reply.ReadableSize();
    reply.PushData(val, 4);
    
    return reply.ReadableSize() - oldSize;
}

size_t Format0(UnboundedBuffer& reply)
{
    const char* val = ":0\r\n";
    
    size_t  oldSize = reply.ReadableSize();
    reply.PushData(val, 4);
    
    return reply.ReadableSize() - oldSize;
}

