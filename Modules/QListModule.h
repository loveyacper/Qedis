#ifndef BERT_QLISTMODULE_H
#define BERT_QLISTMODULE_H

#include <vector>
#include "QString.h"
#include "QCommon.h"

namespace qedis
{
class UnboundedBuffer;
}


// ldel  list_key  item_index
// For delete list item by index
//
extern "C"
qedis::QError ldel(const std::vector<qedis::QString>& params, qedis::UnboundedBuffer* reply);

#endif

