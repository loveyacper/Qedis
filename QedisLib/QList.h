#ifndef BERT_QLIST_H
#define BERT_QLIST_H

#include "QString.h"
#include <list>

namespace qedis
{

enum class ListPosition
{
    head,
    tail,
};

using QList = std::list<QString>;

QObject  CreateListObject();

}

#endif

