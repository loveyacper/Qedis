#ifndef BERT_QDUMPINTERFACE_H
#define BERT_QDUMPINTERFACE_H

#include <stdint.h>
#include "QString.h"

namespace qedis
{

struct QObject;

class QDumpInterface
{
public:
    virtual ~QDumpInterface() {}

    virtual QObject Get(const QString& key) = 0;
    virtual bool Put(const QString& key, const QObject& obj, int64_t ttl = 0) = 0;
    virtual bool Put(const QString& key) = 0;
    virtual bool Delete(const QString& key) = 0;

    //std::vector<QObject> MultiGet(const QString& key);
    //bool MultiPut(const QString& key, const QObject& obj, int64_t ttl = 0);
    //SaveAllRedisTolevelDb();
    //LoadAllLeveldbToRedis();
};

}

#endif

