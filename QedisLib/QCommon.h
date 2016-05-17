#ifndef BERT_QCOMMON_H
#define BERT_QCOMMON_H

#include <cstddef>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <functional>
#include <strings.h>
#include "QString.h"

#define QEDIS static_cast<Qedis* >(Server::Instance())

#define CRLF "\r\n"


namespace qedis
{

const int kStringMaxBytes = 1 * 1024 * 1024 * 1024;
    
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

inline const char* EncodingStringInfo(unsigned encode)
{
    switch (encode)
    {
        case QEncode_raw:
            return "raw";
            
        case QEncode_int:
            return "int";
            
        case QEncode_list:
            return "list";
            
        case QEncode_set:
            return "set";
            
        case QEncode_hash:
            return "hash";
            
        case QEncode_sset:
            return "sset";
            
        default:
            break;
    }
    
    return "unknown";
}

enum QError
{
    QError_nop       = -1,
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
    QError_readonlySlave = 12,
    QError_needAuth  = 13,
    QError_errAuth   = 14,
    QError_nomodule   = 15,
    QError_moduleinit = 16,
    QError_modulerepeat = 17,
    QError_max,
};

extern struct QErrorInfo
{
    int len;
    const char* errorStr;
} g_errorInfo[] ;

template <typename T>
inline std::size_t Number2Str(char* ptr, std::size_t nBytes, T val)
{
    if (!ptr || !nBytes)
        return 0;

    bool negative = false;
    if (val < 0)
    {
        negative = true;
        val = -val;
    }

    std::size_t off = 0;
    while (val > 0)
    {
        if (off >= nBytes)
            return 0;

        ptr[off ++] = val % 10 + '0';
        val /= 10;
    }

    if (negative)
    {
        if (off >= nBytes)
            return 0;

        ptr[off ++] = '-';
    }

    std::reverse(ptr, ptr + off);

    return off;
}

int         Double2Str(char* ptr, std::size_t nBytes, double val);
bool        TryStr2Long(const char* ptr, std::size_t nBytes, long& val); // only for decimal
bool        Strtol(const char* ptr, std::size_t nBytes, long* outVal);
bool        Strtoll(const char* ptr, std::size_t nBytes, long long* outVal);
bool        Strtof(const char* ptr, std::size_t nBytes, float* outVal);
bool        Strtod(const char* ptr, std::size_t nBytes, double* outVal);
const char* Strstr(const char* ptr, std::size_t nBytes, const char* pattern, std::size_t nBytes2);
const char* SearchCRLF(const char* ptr, std::size_t nBytes);


class UnboundedBuffer;

std::size_t FormatInt(long value, UnboundedBuffer* reply);
std::size_t FormatSingle(const char* str, std::size_t len, UnboundedBuffer* reply);
std::size_t FormatSingle(const QString& str, UnboundedBuffer* reply);
std::size_t FormatBulk(const char* str, std::size_t len, UnboundedBuffer* reply);
std::size_t FormatBulk(const QString& str, UnboundedBuffer* reply);
std::size_t PreFormatMultiBulk(std::size_t nBulk, UnboundedBuffer* reply);

std::size_t FormatEmptyBulk(UnboundedBuffer* reply);
std::size_t FormatNull(UnboundedBuffer* reply);
std::size_t FormatNullArray(UnboundedBuffer* reply);
std::size_t FormatOK(UnboundedBuffer* reply);
std::size_t Format1(UnboundedBuffer* reply);
std::size_t Format0(UnboundedBuffer* reply);

void  ReplyError(QError err, UnboundedBuffer* reply);

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
    
    if (end >= static_cast<long>(size))  end = size - 1;
}

struct NocaseComp
{
    bool operator() (const QString& s1, const QString& s2) const
    {
        return strcasecmp(s1.c_str(), s2.c_str()) < 0;
    }

    bool operator() (const char* s1, const QString& s2) const
    {
        return strcasecmp(s1, s2.c_str()) < 0;
    }

    bool operator() (const QString& s1, const char* s2) const
    {
        return strcasecmp(s1.c_str(), s2) < 0;
    }
};

enum class QParseResult : int8_t
{
    ok,
    wait,
    error,
};

QParseResult  GetIntUntilCRLF(const char*& ptr, std::size_t nBytes, int& val);

std::vector<QString>  SplitString(const QString& str, char seperator);
    
// The defer class for C++11
class ExecuteOnScopeExit
{
public:
    ExecuteOnScopeExit() { }
    
    ExecuteOnScopeExit(ExecuteOnScopeExit&& e)
    {
        func_ = std::move(e.func_);
    }
    
    ExecuteOnScopeExit(const ExecuteOnScopeExit& e) = delete;
    void operator=(const ExecuteOnScopeExit& f) = delete;
    
    template <typename F, typename... Args>
    ExecuteOnScopeExit(F&& f, Args&&... args)
    {
        auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        func_ = [temp]() { (void)temp(); };
    }
    
    ~ExecuteOnScopeExit() noexcept
    {
        if (func_)  func_();
    }
    
private:
    std::function<void ()> func_;
};
    
#define CONCAT(a, b) a##b
#define _MAKE_DEFER_HELPER_(line)  qedis::ExecuteOnScopeExit CONCAT(defer, line) = [&]()


#define QEDIS_DEFER _MAKE_DEFER_HELPER_(__LINE__)


}

#endif

