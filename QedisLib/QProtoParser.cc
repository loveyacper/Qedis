


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
    params_.clear();
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
    while (static_cast<int>(results.size()) < multi_)
    {
        QString res;
        auto parseRet = _ParseStr(ptr, end, res);

        if (parseRet == QParseResult::ok)
        {
            results.emplace_back(std::move(res));
        }
        else
        {
            return parseRet;
        }
    }

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

    return _ParseStrval(ptr, end, result);
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

