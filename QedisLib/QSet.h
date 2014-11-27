#ifndef BERT_QSET_H
#define BERT_QSET_H

#include "util.h"

#if defined(__APPLE__)

#include <unordered_set>
typedef std::unordered_set<QString,
        my_hash,
        std::equal_to<QString> >  QSet;

#else

#include <tr1/unordered_set>
typedef std::tr1::unordered_set<QString,
        my_hash,
        std::equal_to<QString> >  QSet;

#endif



#endif

