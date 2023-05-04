#include "Log/Logger.h"

#include <algorithm>
#include "QStore.h"
#include "QCommand.h"
#include "QConfig.h"
#include "QSlowLog.h"
#include "QClient.h"

namespace qedis
{

QClient*  QClient::s_current = 0;

std::set<std::weak_ptr<QClient>, std::owner_less<std::weak_ptr<QClient> > >
          QClient::s_monitors;

PacketLength QClient::_ProcessInlineCmd(const char* buf,
                                        size_t bytes,
                                        std::vector<QString>& params)
{
    if (bytes < 2)
        return 0;

    QString res;

    for (size_t i = 0; i + 1 < bytes; ++ i)
    {
        if (buf[i] == '\r' && buf[i+1] == '\n')
        {
            if (!res.empty())
                params.emplace_back(std::move(res));

            return static_cast<PacketLength>(i + 2);
        }

        if (isblank(buf[i]))
        {
            if (!res.empty())
            {
                params.reserve(4);
                params.emplace_back(std::move(res));
            }
        }
        else
        {
            res.reserve(16);
            res.push_back(buf[i]);
        }
    }

    return 0;
}

static int ProcessMaster(const char* start, const char* end)
{
    auto state = QREPL.GetMasterState();

    switch (state)
    {
        case QReplState_connected:
            // discard all requests before sync;
            // or continue serve with old data? TODO
            return static_cast<int>(end - start);

        case QReplState_wait_auth:
            if (end - start >= 5)
            {
                if (strncasecmp(start, "+OK\r\n", 5) == 0)
                {
                    QClient::Current()->SetAuth();
                    return 5;
                }
                else
                {
                    assert (!!!"check masterauth config, master password maybe wrong");
                }
            }
            else
            {
                return 0;
            }
            break;

        case QReplState_wait_replconf:
            if (end - start >= 5)
            {
                if (strncasecmp(start, "+OK\r\n", 5) == 0)
                {
                    return 5;
                }
                else
                {
                    assert (!!!"check error: send replconf command");
                }
            }
            else
            {
                return 0;
            }
            break;

        case QReplState_wait_rdb:
        {
            const char* ptr = start;
            //recv RDB file
            if (QREPL.GetRdbSize() == std::size_t(-1))
            {
                ++ ptr; // skip $
                int s;
                if (QParseResult::ok == GetIntUntilCRLF(ptr, end - ptr, s))
                {
                    assert (s > 0); // check error for your masterauth or master config

                    QREPL.SetRdbSize(s);
                    USR << "recv rdb size " << s;
                }
            }
            else
            {
                std::size_t rdb = static_cast<std::size_t>(end - ptr);
                QREPL.SaveTmpRdb(ptr, rdb);
                ptr += rdb;
            }

            return static_cast<int>(ptr - start);
        }
            break;
            
        case QReplState_online:
            break;
    
        default:
            assert(!!!"wrong master state");
            break;
    }
    
    return -1; // do nothing
}

PacketLength QClient::_HandlePacket(const char* start, std::size_t bytes)
{
    s_current = this;

    const char* const end   = start + bytes;
    const char* ptr  = start;
    
    if (GetPeerAddr() == QREPL.GetMasterAddr())
    {
        // check slave state
        auto recved = ProcessMaster(start, end);
        if (recved != -1)
            return static_cast<PacketLength>(recved);
    }

    auto parseRet = parser_.ParseRequest(ptr, end);
    if (parseRet == QParseResult::error)
    {
        if (!parser_.IsInitialState())
        {
            this->OnError(); 
            return 0;
        }

        // try inline command
        std::vector<QString> params;
        auto len = _ProcessInlineCmd(ptr, bytes, params); 
        if (len == 0)
            return 0;

        ptr += len;
        parser_.SetParams(params);
        parseRet = QParseResult::ok;
    }
    else if (parseRet != QParseResult::ok)
    {
        return static_cast<PacketLength>(ptr - start);
    }
    
    QEDIS_DEFER
    {
        _Reset();
    };
    
    // handle packet
    const auto& params = parser_.GetParams();
    if (params.empty())
        return static_cast<PacketLength>(ptr - start);

    QString cmd(params[0]);
    std::transform(params[0].begin(), params[0].end(), cmd.begin(), ::tolower);

    if (!auth_)
    {
        if (cmd == "auth")
        {
            auto now = ::time(nullptr);
            if (now <= lastauth_ + 1)
            {
                // avoid guess password.
                OnError();
                return 0;
            }
            else
            {
                lastauth_ = now;
            }
        }
        else
        {
            ReplyError(QError_needAuth, &reply_);
            SendPacket(reply_);
            return static_cast<PacketLength>(ptr - start);
        }
    }
    
    DBG << "client " << GetID() << ", cmd " << cmd;
    
    QSTORE.SelectDB(db_);
    FeedMonitors(params);
    
    const QCommandInfo* info = QCommandTable::GetCommandInfo(cmd);

    if (!info)
    {
        ReplyError(QError_unknowCmd, &reply_);
        SendPacket(reply_);
        return static_cast<PacketLength>(ptr - start);
    }

    // check transaction
    if (IsFlagOn(ClientFlag_multi))
    {
        if (cmd != "multi" &&
            cmd != "exec" &&
            cmd != "watch" &&
            cmd != "unwatch" &&
            cmd != "discard")
        {
            if (!info->CheckParamsCount(static_cast<int>(params.size())))
            {
                ERR << "queue failed: cmd " << cmd.c_str() << " has params " << params.size();
                ReplyError(info ? QError_param : QError_unknowCmd, &reply_);
                SendPacket(reply_);
                FlagExecWrong();
            }
            else
            {
                if (!IsFlagOn(ClientFlag_wrongExec))
                    queueCmds_.push_back(params);
                
                SendPacket("+QUEUED\r\n", 9);
                INF << "queue cmd " << cmd.c_str();
            }
            
            return static_cast<PacketLength>(ptr - start);
        }
    }
    
    // check readonly slave and execute command
    QError err = QError_ok;
    if (QREPL.GetMasterState() != QReplState_none &&
        !IsFlagOn(ClientFlag_master) &&
        (info->attr & QCommandAttr::QAttr_write))
    {
        ReplyError(err = QError_readonlySlave, &reply_);
    }
    else
    {
        QSlowLog::Instance().Begin();
        err = QCommandTable::ExecuteCmd(params,
                                        info,
                                        IsFlagOn(ClientFlag_master) ? nullptr : &reply_);
        QSlowLog::Instance().EndAndStat(params);
    }
    
    SendPacket(reply_);
    
    if (err == QError_ok && (info->attr & QAttr_write))
    {
        Propogate(params);
    }
    
    return static_cast<PacketLength>(ptr - start);
}

QClient*  QClient::Current()
{
    return s_current;
}

QClient::QClient() : db_(0), flag_(0), name_("clientxxx")
{
    auth_ = false;
    SelectDB(0);
    _Reset();
}

void QClient::OnConnect()
{
    const bool peerIsMaster = (GetPeerAddr() == QREPL.GetMasterAddr());

    if (peerIsMaster)
    {
        QREPL.SetMasterState(QReplState_connected);
        QREPL.SetMaster(std::static_pointer_cast<QClient>(shared_from_this()));
            
        SetName("MasterConnection"); 
        SetFlag(ClientFlag_master);

        if (g_config.masterauth.empty())
            SetAuth();
    }
    else
    {
        if (g_config.password.empty())
            SetAuth();
    }
}

bool QClient::SelectDB(int db)
{ 
    if (QSTORE.SelectDB(db) >= 0)
    {
        db_ = db;
        return true;
    }

    return false;
}

void QClient::_Reset()
{
    s_current = 0;

    parser_.Reset();
    reply_.Clear();
}

bool QClient::Watch(int dbno, const QString& key)
{
    DBG << "Client " << name_ << " watch " << key.c_str() << ", db " << dbno;
    return watchKeys_[dbno].insert(key).second;
}

bool QClient::NotifyDirty(int dbno, const QString& key)
{
    if (IsFlagOn(ClientFlag_dirty))
    {
        INF << "client is already dirty " << GetID();
        return true;
    }
    
    if (watchKeys_[dbno].count(key))
    {
        INF << GetID() << " client become dirty because key " << key << " in db " << dbno;
        SetFlag(ClientFlag_dirty);
        return true;
    }
    else
    {
        INF << "Dirty key is not exist: " << key << ", because client unwatch before dirty";
    }
    
    return false;
}

bool QClient::Exec()
{
    QEDIS_DEFER
    {
        this->ClearMulti();
        this->ClearWatch();
    };
    
    if (IsFlagOn(ClientFlag_wrongExec))
    {
        return false;
    }
    
    if (IsFlagOn(ClientFlag_dirty))
    {
        FormatNullArray(&reply_);
        return true;
    }
    
    PreFormatMultiBulk(queueCmds_.size(), &reply_);
    for (const auto& cmd : queueCmds_)
    {
        DBG << "EXEC " << cmd[0] << ", for client " << GetID();
        const QCommandInfo* info = QCommandTable::GetCommandInfo(cmd[0]);
        QError err = QCommandTable::ExecuteCmd(cmd, info, &reply_);
        
        // may dirty clients;
        if (err == QError_ok && (info->attr & QAttr_write))
        {
            Propogate(cmd);
        }
    }
    
    return true;
}

void QClient::ClearMulti()
{
    queueCmds_.clear();
    ClearFlag(ClientFlag_multi);
    ClearFlag(ClientFlag_wrongExec);
}
    
void QClient::ClearWatch()
{
    watchKeys_.clear();
    ClearFlag(ClientFlag_dirty);
}


bool  QClient::WaitFor(const QString& key, const QString* target)
{
    bool  succ = waitingKeys_.insert(key).second;
    
    if (succ && target)
    {
        if (!target_.empty())
        {
            ERR << "Wait failed for key " << key << ", because old target " << target_;
            waitingKeys_.erase(key);
            return false;
        }
        
        target_ = *target;
    }
    
    return succ;
}


void   QClient::SetSlaveInfo()
{
    slaveInfo_.reset(new QSlaveInfo());
}

void  QClient::AddCurrentToMonitor()
{
    s_monitors.insert(std::static_pointer_cast<QClient>(s_current->shared_from_this()));
}

void  QClient::FeedMonitors(const std::vector<QString>& params)
{
    assert(!params.empty());

    if (s_monitors.empty())
        return;

    char buf[512];
    int n = snprintf(buf, sizeof buf, "+[db%d %s:%hu]: \"",
             QSTORE.GetDB(),
             s_current->peerAddr_.GetIP(),
             s_current->peerAddr_.GetPort());

    assert(n > 0);
    
    for (const auto& e : params)
    {
        if (static_cast<int>(sizeof buf) > n)
            n += snprintf(buf + n, sizeof buf - n, "%s ", e.data());
        else
            break;
    }
    
    -- n; // no space follow last param
    
    for (auto it(s_monitors.begin()); it != s_monitors.end(); )
    {
        auto  m = it->lock();
        if (m)
        {
            m->SendPacket(buf, n);
            m->SendPacket("\"" CRLF, 3);
            
            ++ it;
        }
        else
        {
            s_monitors.erase(it ++);
        }
    }
}
    
}

