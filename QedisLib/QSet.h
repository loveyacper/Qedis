#ifndef BERT_QSET_H
#define BERT_QSET_H

#include "util.h"
#include <unordered_set>

typedef std::unordered_set<QString,
        my_hash,
        std::equal_to<QString> >  QSet;

QObject CreateSetObject();

#endif

