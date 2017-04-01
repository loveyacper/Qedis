#ifndef BERT_QPROTOPARSER_H
#define BERT_QPROTOPARSER_H

#include <vector>
#include "QString.h"

namespace qedis
{

class QProtoParser
{
public:
    void Reset();
    QParseResult ParseRequest(const char*& ptr, const char* end);

    const std::vector<QString>& GetParams() const { return params_; }
    void SetParams(std::vector<QString> p) { params_ = std::move(p); }
    
    bool IsInitialState() const { return multi_ == -1; }

private:
    QParseResult _ParseMulti(const char*& ptr, const char* end, int& result);
    QParseResult _ParseStrlist(const char*& ptr, const char* end, std::vector<QString>& results);
    QParseResult _ParseStr(const char*& ptr, const char* end, QString& result);
    QParseResult _ParseStrval(const char*& ptr, const char* end, QString& result);
    QParseResult _ParseStrlen(const char*& ptr, const char* end, int& result);

    int multi_ = -1;
    int paramLen_ = -1;

    size_t numOfParam_ = 0; // for optimize
    std::vector<QString> params_;
};

}

#endif
