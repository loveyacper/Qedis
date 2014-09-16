#ifndef BERT_QCOMMON_H
#define BERT_QCOMMON_H

//#include "QString.h"
#include <cstddef>

#define QEDIS static_cast<Qedis* >(Server::Instance())

class   Logger;
extern  Logger*  g_logger;

enum QType
{
    QType_invalid,
    QType_string,
    QType_list,
    QType_unorderSet,
    QType_set,
    QType_hash,
    // < 16
};

enum QEncode
{
    QEncode_raw, // string
    QEncode_int, // string as int

    QEncode_list,
    QEncode_ziplist,

    QEncode_hash,

    QEncode_intset,
    QEncode_zipmap,
};

enum QError
{
    QError_ok        = 0,
    QError_wrongType = 1,
    QError_exist     = 2,
    QError_notExist  = 3,
    QError_paramNotMatch = 4,
    QError_unknowCmd = 5,
    QError_max,
};

extern struct QErrorInfo
{
    int len;
    const char* errorStr;
} g_errorInfo[] ;

void        Int2Str(char* ptr, std::size_t nBytes, long val);
bool        Str2Int(const char* ptr, std::size_t nBytes, long& val); // only for decimal
bool        Strtol(const char* ptr, std::size_t nBytes, long* outVal);
bool        Strtof(const char* ptr, std::size_t nBytes, float* outVal);
const char* Strstr(const char* ptr, std::size_t nBytes, const char* pattern, std::size_t nBytes2);
const char* SearchCRLF(const char* ptr, std::size_t nBytes);


class UnboundedBuffer;

std::size_t FormatInt(long value, UnboundedBuffer& reply);
std::size_t FormatSingle(const char* str, std::size_t len, UnboundedBuffer& reply);
std::size_t FormatError(const char* str, std::size_t len, UnboundedBuffer& reply);
std::size_t FormatBulk(const char* str, std::size_t len, UnboundedBuffer& reply);
std::size_t PreFormatMultiBulk(std::size_t nBulk, UnboundedBuffer& reply);

std::size_t FormatNull(UnboundedBuffer& reply);
std::size_t FormatNullArray(UnboundedBuffer& reply);
std::size_t FormatOK(UnboundedBuffer& reply);

void  ReplyErrorInfo(QError err, UnboundedBuffer& reply);

#endif

