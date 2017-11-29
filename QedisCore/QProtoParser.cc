


#include "QCommon.h"
#include "QProtoParser.h"

#include <assert.h>

// 1 request -> multi strlist
// 2 multi -> * number crlf
// 3 strlist -> str strlist | empty
// 4 str -> strlen strval
// 5 strlen -> $ number crlf
// 6 strval -> string crlf

namespace qedis
{

void QProtoParser::Reset()
{
    multi_ = -1;
    paramLen_ = -1;
    numOfParam_ = 0;

    // Optimize: Most redis command has 3 args
    while (params_.size() > 3)
        params_.pop_back();
}

QParseResult QProtoParser::ParseRequest(const char*& ptr, const char* end)
{
    if (multi_ == -1)
    {
        auto parseRet = _ParseMulti(ptr, end, multi_);
        if (parseRet == QParseResult::error ||
            multi_ < -1)
            return QParseResult::error;

        if (parseRet != QParseResult::ok)
            return QParseResult::wait;
    }

    return _ParseStrlist(ptr, end, params_);
}

QParseResult QProtoParser::_ParseMulti(const char*& ptr, const char* end, int& result)
{
    if (end - ptr < 3)
        return QParseResult::wait;

    if (*ptr != '*')
        return QParseResult::error;

    ++ ptr;

    return GetIntUntilCRLF(ptr,  end - ptr, result);
}

QParseResult QProtoParser::_ParseStrlist(const char*& ptr, const char* end, std::vector<QString>& results)
{
    while (static_cast<int>(numOfParam_) < multi_)
    {
        if (results.size() < numOfParam_ + 1)
            results.resize(numOfParam_ + 1);

        auto parseRet = _ParseStr(ptr, end, results[numOfParam_]);

        if (parseRet == QParseResult::ok)
        {
            ++ numOfParam_;
        }
        else
        {
            return parseRet;
        }
    }

    results.resize(numOfParam_);
    return QParseResult::ok;
}

QParseResult QProtoParser::_ParseStr(const char*& ptr, const char* end, QString& result)
{
    if (paramLen_ == -1)
    {
        auto parseRet = _ParseStrlen(ptr, end, paramLen_);
        if (parseRet == QParseResult::error ||
            paramLen_ < -1)
            return QParseResult::error;

        if (parseRet != QParseResult::ok)
            return QParseResult::wait;
    }

    if (paramLen_ == -1)
    {
        result.clear(); // or should be "(nil)" ?
        return QParseResult::ok;
    }
    else
    {
        return _ParseStrval(ptr, end, result);
    }
}

QParseResult QProtoParser::_ParseStrval(const char*& ptr, const char* end, QString& result)
{
    assert (paramLen_ >= 0);

    if (static_cast<int>(end - ptr) < paramLen_ + 2)
        return QParseResult::wait;

    auto tail = ptr + paramLen_;
    if (tail[0] != '\r' || tail[1] != '\n')
        return QParseResult::error;

    result.assign(ptr, tail - ptr);
    ptr = tail + 2;
    paramLen_ = -1;

    return QParseResult::ok;
}

QParseResult QProtoParser::_ParseStrlen(const char*& ptr, const char* end, int& result)
{
    if (end - ptr < 3)
        return QParseResult::wait;

    if (*ptr != '$')
        return QParseResult::error;

    ++ ptr;

    const auto ret = GetIntUntilCRLF(ptr,  end - ptr, result);
    if (ret != QParseResult::ok)
        -- ptr;

    return ret;
}

}

