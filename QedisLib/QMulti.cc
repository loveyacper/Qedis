#include "QMulti.h"
#include "QClient.h"
#include "Log/Logger.h"
#include "Timer.h"

using namespace std;

QMulti&    QMulti::Instance()
{
    static QMulti  mt;
    return mt;
}
    
void  QMulti::Watch(QClient* client, const QString& key)
{
    if (client->Watch(key))
    {
        Clients& cls = m_clients[key];
        cls.push_back(std::static_pointer_cast<QClient>(client->shared_from_this()));
    }
}


void QMulti::Unwatch(QClient* client)
{
    client->UnWatch();
}


void QMulti::Multi(QClient* client)
{
    client->ClearMulti();
    client->SetFlag(ClientFlag_multi);
}

bool QMulti::Exec(QClient* client)
{
    bool succ = client->Exec();
    client->ClearMulti();
    client->UnWatch();
    return  succ;
}

void QMulti::Discard(QClient* client)
{
    client->ClearMulti();
    client->UnWatch();
}


void  QMulti::NotifyDirty(const QString& key)
{
    INF << "Try NotifyDirty " << key.c_str();
    WatchedClients::iterator it = m_clients.find(key);
    if (it != m_clients.end())
    {
        Clients& cls = it->second;
        for (Clients::iterator itCli(cls.begin());
             itCli != cls.end();  )
        {
            std::shared_ptr<QClient>  client(itCli->lock());
            if (!client)
            {
                WRN << "erase not exist client ";
                cls.erase(itCli ++);
            }
            else
            {
                if (!client->NotifyDirty(key))
                {
                    WRN << "erase dirty client ";
                    cls.erase(itCli ++);
                }
                else
                {
                    ++ itCli;
                }
            }
        }
        
        if (cls.empty())
        {
            m_clients.erase(it);
        }
    }
}

// multi commands
QError  watch(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();
    if (client->IsFlagOn(ClientFlag_multi))
    {
        ReplyError(QError_watch, reply);
        return  QError_watch;
    }
    
    for (size_t i = 1; i < params.size(); ++ i)
    {
        QMulti::Instance().Watch(client, params[i]);
    }
    
    FormatOK(reply);
    return QError_ok;
}

QError  unwatch(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();
    QMulti::Instance().Unwatch(client);
    FormatOK(reply);
    return QError_ok;
}

QError  multi(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();
    QMulti::Instance().Multi(client);
    FormatOK(reply);
    return QError_ok;
}

QError  exec(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();
    if (!client->IsFlagOn(ClientFlag_multi))
    {
        ReplyError(QError_noMulti, reply);
        return QError_noMulti;
    }
    if (!QMulti::Instance().Exec(client))
    {
        ReplyError(QError_dirtyExec, reply);
        return  QError_dirtyExec;
    }
    return QError_ok;
}

QError  discard(const vector<QString>& params, UnboundedBuffer& reply)
{
    QClient* client = QClient::Current();
    QMulti::Instance().Discard(client);
    FormatOK(reply);
    return QError_ok;
}
