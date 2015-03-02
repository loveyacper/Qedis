#ifndef BERT_QCOMMON_H
#define BERT_QCOMMON_H

#include <cstddef>

#define QEDIS static_cast<Qedis* >(Server::Instance())


enum QType
{
    QType_invalid,
    QType_string,
    QType_list,
    QType_set,
    QType_sortedSet,
    QType_hash,
    // < 16
};

enum QEncode
{
    QEncode_invalid,

    QEncode_raw, // string
    QEncode_int, // string as int

    QEncode_list,
    
    QEncode_set,
    QEncode_hash,
    
    QEncode_sset,
};

enum QError
{
    QError_ok        = 0,
    QError_type      = 1,
    QError_exist     = 2,
    QError_notExist  = 3,
    QError_param     = 4,
    QError_unknowCmd = 5,
    QError_nan       = 6,
    QError_syntax    = 7,
    QError_dirtyExec = 8,
    QError_watch     = 9,
    QError_noMulti   = 10,
    QError_invalidDB = 11,
    QError_max,
};

extern struct QErrorInfo
{
    int len;
    const char* errorStr;
} g_errorInfo[] ;

int         Int2Str(char* ptr, std::size_t nBytes, long val);
int         Double2Str(char* ptr, std::size_t nBytes, double val);
bool        Str2Long(const char* ptr, std::size_t nBytes, long& val); // only for decimal
bool        Strtol(const char* ptr, std::size_t nBytes, long* outVal);
bool        Strtoll(const char* ptr, std::size_t nBytes, long long* outVal);
bool        Strtof(const char* ptr, std::size_t nBytes, float* outVal);
bool        Strtod(const char* ptr, std::size_t nBytes, double* outVal);
const char* Strstr(const char* ptr, std::size_t nBytes, const char* pattern, std::size_t nBytes2);
const char* SearchCRLF(const char* ptr, std::size_t nBytes);


class UnboundedBuffer;

std::size_t FormatInt(long value, UnboundedBuffer& reply);
std::size_t FormatSingle(const char* str, std::size_t len, UnboundedBuffer& reply);
std::size_t FormatBulk(const char* str, std::size_t len, UnboundedBuffer& reply);
std::size_t PreFormatMultiBulk(std::size_t nBulk, UnboundedBuffer& reply);

std::size_t FormatNull(UnboundedBuffer& reply);
std::size_t FormatNullArray(UnboundedBuffer& reply);
std::size_t FormatOK(UnboundedBuffer& reply);
std::size_t Format1(UnboundedBuffer& reply);
std::size_t Format0(UnboundedBuffer& reply);

void  ReplyError(QError err, UnboundedBuffer& reply);

inline void AdjustIndex(long& start, long& end, size_t  size)
{
    if (size == 0)
    {
        end = 0, start = 1;
        return;
    }
    
    if (start < 0)  start += size;
    if (start < 0)  start = 0;
    if (end < 0)    end += size;
    
    if (start > end || start >= static_cast<long>(size))
        end = 0, start = 1;
    
    if (end >= static_cast<long>(size))  end = size - 1;
}

#endif

