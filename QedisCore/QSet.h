#ifndef BERT_QSET_H
#define BERT_QSET_H

#include "QHelper.h"
#include <unordered_set>

namespace qedis
{

using QSet = std::unordered_set<QString, Hash>;

size_t SScanKey(const QSet& qset, size_t cursor, size_t count, std::vector<QString>& res);
    
}

#endif

