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

typedef std::list<QString>  QList;

QObject  CreateListObject();

}

#endif

