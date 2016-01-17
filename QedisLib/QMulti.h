#ifndef BERT_QMULTI_H
#define BERT_QMULTI_H

#include <map>
#include <vector>
#include <memory>
#include "QString.h"

namespace qedis
{

class QClient;
class QMulti
{
public:
    static QMulti& Instance();

    void  Watch(QClient* client, int dbno, const QString& key);
    void  Unwatch(QClient* client);
    void  Multi(QClient* client);
    bool  Exec(QClient* client);
    void  Discard(QClient* client);

    void  NotifyDirty(int dbno, const QString& key);
private:
    QMulti() {}

    using Clients = std::vector<std::weak_ptr<QClient> >;
    using WatchedClients = std::map<int, std::map<QString, Clients> >;
    
    WatchedClients  clients_;
};

}

#endif

