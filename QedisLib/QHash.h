#ifndef BERT_QHASH_H
#define BERT_QHASH_H

#include "QString.h"
#include "util.h"

#include <unordered_map>
typedef std::unordered_map<QString, QString,
        my_hash,
        std::equal_to<QString> >  QHash;


QObject  CreateHashObject();


#endif

