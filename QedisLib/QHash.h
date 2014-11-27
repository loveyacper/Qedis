#ifndef BERT_QHASH_H
#define BERT_QHASH_H

#include "QString.h"

#if defined(__APPLE__)

#include "util.h"
#include <unordered_map>
typedef std::unordered_map<QString, QString,
        my_hash,
        std::equal_to<QString> >  QHash;
/*typedef std::unordered_map<QString, QString,
        my_hash,
        std::equal_to<QString>,
        Bert::Allocator<QString> >  QHash;*/

#else

#include <tr1/unordered_map>
typedef std::tr1::unordered_map<QString, QString,
        my_hash,
        std::equal_to<QString> >  QHash;

#endif



#endif

