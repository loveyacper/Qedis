#ifndef BERT_QMULTI_H
#define BERT_QMULTI_H

#include <map>
#include <vector>
#include "QString.h"
#include <memory>

class QClient;
class QMulti
{
public:
    static QMulti& Instance();

    void  Watch(QClient* client, const QString& key);
    void  Unwatch(QClient* client);
    void  Multi(QClient* client);
    bool  Exec(QClient* client);
    void  Discard(QClient* client);

    void  NotifyDirty(const QString& key);
  //  void InitPubsubTimer();
    //void RecycleClients(QString& startChannel, QString& startPattern);

private:
    QMulti() {}

    typedef std::vector<std::weak_ptr<QClient> >   Clients;
    typedef std::map<QString, Clients>    WatchedClients;
    
    WatchedClients  m_clients;
};

#endif

