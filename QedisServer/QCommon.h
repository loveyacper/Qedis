#ifndef BERT_QCOMMON_H
#define BERT_QCOMMON_H

//#include "QString.h"

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

void        Int2Str(char* ptr, int nBytes, long val);
bool        Str2Int(const char* ptr, int nBytes, long& val); // only for decimal
bool        Strtol(const char* ptr, int nBytes, long* outVal);
bool        Strtof(const char* ptr, int nBytes, float* outVal);
const char* Strstr(const char* ptr, int nBytes, const char* pattern, int nBytes2);
const char* SearchCRLF(const char* ptr, int nBytes);


class UnboundedBuffer;

int FormatInt(int value, UnboundedBuffer& reply);
int FormatSingle(const char* str, int len, UnboundedBuffer& reply);
int FormatError(const char* str, int len, UnboundedBuffer& reply);
int FormatBulk(const char* str, int len, UnboundedBuffer& reply);
int PreFormatMultiBulk(int nBulk, UnboundedBuffer& reply);

int FormatNull(UnboundedBuffer& reply);

void  ReplyErrorInfo(QError err, UnboundedBuffer& reply);

#endif

