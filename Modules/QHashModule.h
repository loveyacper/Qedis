#ifndef BERT_QHASHMODULE_H
#define BERT_QHASHMODULE_H

#include <vector>
#include "QString.h"
#include "QCommon.h"

namespace qedis
{
class UnboundedBuffer;
}


// hgets  hash_key  filter_pattern
// for example, `hgets profile c*y` may return ["city":"shanghai", "country":"china"]
// because c*y matches "city" and "country"
//
extern "C"
qedis::QError hgets(const std::vector<qedis::QString>& params, qedis::UnboundedBuffer* reply);

#endif

