#include "QCommon.h"
#include "UnboundedBuffer.h"
#include <limits>
#include <stdlib.h>
#include <errno.h>
#include <algorithm>
#include <iostream>


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
    
    {sizeof "-EXECABORT Transaction discarded because of previous errors.\r\n"-1, "-EXECABORT Transaction discarded because of previous errors.\r\n"},
    {sizeof "-WATCH inside MULTI is not allowed\r\n"-1, "-WATCH inside MULTI is not allowed\r\n"},
    {sizeof "-EXEC without MULTI\r\n"-1, "-EXEC without MULTI\r\n"},
    {sizeof "-ERR invalid DB index\r\n"-1, "-ERR invalid DB index\r\n"}
};


int Double2Str(char* ptr, std::size_t nBytes, double val)
{
    return snprintf(ptr, nBytes - 1, "%.6g", val);
}

bool TryStr2Long(const char* ptr, size_t nBytes, long& val)
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
        
        if (!negtive && val > std::numeric_limits<long>::max() / 10)
        {
            std::cerr << "long will overflow " << val << std::endl;
            return false;
        }
        
        if (negtive && val > (-(std::numeric_limits<long>::min() + 1)) / 10)
        {
            std::cerr << "long will underflow " << val << std::endl;
            return false;
        }

        val *= 10;
        
        if (!negtive && val > std::numeric_limits<long>::max() - ( ptr[i] - '0'))
        {
            std::cerr << "long will overflow " << val << std::endl;
            return false;
        }
        
        
        if (negtive && (val - 1) > (-(std::numeric_limits<long>::min() + 1)) - (ptr[i] - '0'))
        {
            std::cerr << "long will underflow " << val << std::endl;
            return false;
        }
        
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
    if (nBytes == 0 || nBytes > 20) // include the sign
        return false;

    errno = 0;
    char* pEnd = 0;
    *outVal = strtol(ptr, &pEnd, 0);

    if (errno == ERANGE ||
        errno == EINVAL)
        return false;

    return pEnd == ptr + nBytes;
}

bool Strtoll(const char* ptr, size_t nBytes, long long* outVal)
{
    if (nBytes == 0 || nBytes > 20)
        return false;
    
    errno  = 0;
    char* pEnd = 0;
    *outVal = strtoll(ptr, &pEnd, 0);
    
    if (errno == ERANGE ||
        errno == EINVAL)
        return false;
    
    return pEnd == ptr + nBytes;
}

bool Strtof(const char* ptr, size_t nBytes, float* outVal)
{
    if (nBytes == 0 || nBytes > 20)
        return false;
    
    errno = 0;
    char* pEnd = 0;
    *outVal = strtof(ptr, &pEnd);
    
    if (errno == ERANGE ||
        errno == EINVAL)
        return false;
    
    return pEnd == ptr + nBytes;
}

bool Strtod(const char* ptr, size_t nBytes, double* outVal)
{
    if (nBytes == 0 || nBytes > 20)
        return false;
    
    errno = 0;
    char* pEnd = 0;
    *outVal = strtod(ptr, &pEnd);
    
    if (errno == ERANGE ||
        errno == EINVAL)
        return false;
    
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

size_t  FormatInt(long value, UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    
    char val[32];
    int len = snprintf(val, sizeof val, "%ld" CRLF, value);
    
    size_t  oldSize = reply->ReadableSize();
    reply->PushData(":", 1);
    reply->PushData(val, len);
    
    return reply->ReadableSize() - oldSize;
}

size_t  FormatSingle(const char* str, size_t len, UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    size_t   oldSize = reply->ReadableSize();
    reply->PushData("+", 1);
    reply->PushData(str, len);
    reply->PushData(CRLF, 2);

    return reply->ReadableSize() - oldSize;
}

size_t  FormatSingle(const QString& str, UnboundedBuffer* reply)
{
    return  FormatSingle(str.c_str(), str.size(), reply);
}

size_t  FormatBulk(const char* str, size_t len, UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    
    size_t oldSize = reply->ReadableSize();
    reply->PushData("$", 1);

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%lu" CRLF, len);
    reply->PushData(val, tmp);

    if (str && len > 0)
    {
        reply->PushData(str, len);
    }
    
    reply->PushData(CRLF, 2);
    
    return reply->ReadableSize() - oldSize;
}

size_t  FormatBulk(const QString& str, UnboundedBuffer* reply)
{
    return  FormatBulk(str.c_str(), str.size(), reply);
}

size_t  PreFormatMultiBulk(size_t nBulk, UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    
    size_t  oldSize = reply->ReadableSize();
    reply->PushData("*", 1);

    char val[32];
    int tmp = snprintf(val, sizeof val - 1, "%lu" CRLF, nBulk);
    reply->PushData(val, tmp);

    return reply->ReadableSize() - oldSize;
}

void  ReplyError(QError err, UnboundedBuffer* reply)
{
    if (!reply)
        return;
    
    const QErrorInfo& info = g_errorInfo[err];

    reply->PushData(info.errorStr, info.len);
}

size_t  FormatNull(UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    
    size_t   oldSize = reply->ReadableSize();
    reply->PushData("$-1" CRLF, 5);
    
    return reply->ReadableSize() - oldSize;
}


size_t  FormatNullArray(UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    
    size_t   oldSize = reply->ReadableSize();
    reply->PushData("*-1" CRLF, 5);
    
    return reply->ReadableSize() - oldSize;
}

size_t  FormatOK(UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    
    size_t   oldSize = reply->ReadableSize();
    reply->PushData("+OK" CRLF, 5);
    
    return reply->ReadableSize() - oldSize;
}

size_t Format1(UnboundedBuffer* reply)
{
    if (!reply)
        return 0;
    
    const char* val = ":1\r\n";
    
    size_t  oldSize = reply->ReadableSize();
    reply->PushData(val, 4);
    
    return reply->ReadableSize() - oldSize;
}

size_t Format0(UnboundedBuffer* reply)
{
    if (!reply)
        return 0;

    const char* val = ":0\r\n";
    
    size_t  oldSize = reply->ReadableSize();
    reply->PushData(val, 4);
    
    return reply->ReadableSize() - oldSize;
}

QParseInt  GetIntUntilCRLF(const char*& ptr, std::size_t nBytes, int& val)
{
    if (nBytes < 3)
        return QParseInt::error;
    
    std::size_t i = 0;
    bool negtive = false;
    if (ptr[0] == '-')
    {
        negtive = true;
        ++ i;
    }
    else if (ptr[0] == '+')
    {
        ++ i;
    }
    
    val = 0;
    for (; i < nBytes; ++ i)
    {
        if (isdigit(ptr[i]))
        {
            val *= 10;
            val += ptr[i] - '0';
        }
        else
        {
            if (ptr[i] != '\r' || (i+1 < nBytes && ptr[i+1] != '\n'))
                return QParseInt::error;
            
            if (i + 1 == nBytes)
                return QParseInt::waitCrlf;
            
            break;
        }
    }
    
    if (negtive)
        val *= -1;
    
    ptr += i;
    return QParseInt::ok;
}

std::vector<QString>  SplitString(const QString& str, char seperator)
{
    std::vector<QString>  results;
    
    QString::size_type start = 0;
    QString::size_type sep = str.find(seperator);
    while (sep != QString::npos)
    {
        if (start < sep)
            results.emplace_back(str.substr(start, sep - start));
        
        start = sep + 1;
        sep   = str.find(seperator, start);
    }
    
    if (start != str.size())
        results.emplace_back(str.substr(start));
    
    return std::move(results);
}
