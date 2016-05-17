#ifndef BERT_QSETMODULE_H
#define BERT_QSETMODULE_H

#include <vector>
#include "QString.h"
#include "QCommon.h"

namespace qedis
{
class UnboundedBuffer;
}


// skeys set_key  filter_pattern
// for example, `skeys cities sh*` may return ["shanghai", "shenzhen"]
//
extern "C"
qedis::QError skeys(const std::vector<qedis::QString>& params, qedis::UnboundedBuffer* reply);

#endif

