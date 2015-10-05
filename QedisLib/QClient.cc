#include "Log/Logger.h"

#include <algorithm>
#include "QStore.h"
#include "QCommand.h"
#include "QMulti.h"
#include "QAOF.h"
#include "QConfig.h"
#include "QSlowLog.h"
#include "QClient.h"

// *3  CR LF
// $4  CR LF
// sadd CR LF
// $5  CR LF
// myset CR LF
// $3   CR LF
// 345 CR LF

QClient*  QClient::s_pCurrentClient = 0;

std::set<std::weak_ptr<QClient>, std::owner_less<std::weak_ptr<QClient> > >
          QClient::s_monitors;

BODY_LENGTH_T QClient::_ProcessInlineCmd(const char* buf, size_t bytes)
{
    if (bytes < 2)
        return 0;
    
    BODY_LENGTH_T  len = 0;
    size_t cursor = bytes - 1;
    for (; cursor > 0; -- cursor)
    {
        if (buf[cursor] == '\n' && buf[cursor-1] == '\r')
        {
            len = static_cast<BODY_LENGTH_T>(cursor + 1);
            m_state = ParseCmdState::Ready;
            break;
        }
    }
    
    if (m_state == ParseCmdState::Ready)
    {
        QString   param;
        for (size_t i = 0; i + 1 < cursor; ++ i)
        {
            if (isblank(buf[i]))
            {
                if (!param.empty())
                {
                    INF << "inline cmd param " << param.c_str();
                    m_params.emplace_back(std::move(param));
                    param.clear();
                }
            }
            else
            {
                param.push_back(buf[i]);
            }
        }
        
        INF << "inline cmd param " << param.c_str();
        m_params.emplace_back(std::move(param));
    }
    
    return len;
}

static void Propogate(const std::vector<QString>& params);

BODY_LENGTH_T QClient::_HandlePacket(AttachedBuffer& buf)
{
    const size_t   bytes = buf.ReadableSize();
    const char* const start = buf.ReadAddr();
    const char* const end   = start + bytes;

    const char* ptr  = start;
    
    {
        auto& repl = QReplication::Instance();
        auto& info = repl.GetMasterInfo();
        
        // discard all data before request sync;
        // or support service use old data? TODO
        if (info.state == QReplState_connected)
            return  static_cast<BODY_LENGTH_T>(bytes);
            
        if (info.state == QReplState_wait_rdb)
        {
            //RECV RDB FILE
            if (info.rdbSize == std::size_t(-1))
            {
                assert(info.state == QReplState_wait_rdb);
     
                ++ ptr; // skip $
                int s;
                if (QParseInt::ok == GetIntUntilCRLF(ptr, end - ptr, s))
                {
                    info.rdbSize = s;
                    USR << "recv rdb size " << s;
                    ptr += 2; // skip CRLF
                }
            }
            else
            {
                auto rdb = bytes;
                if (rdb > info.rdbSize)
                    rdb = info.rdbSize;
                
                QReplication::Instance().SaveTmpRdb(start, rdb);
                ptr += rdb;
            }

            return static_cast<BODY_LENGTH_T>(ptr - start);
        }
    }
    
    QParseInt parseIntRet = QParseInt::ok;
    
    switch (m_state)
    {
        case ParseCmdState::Init:
            assert (m_multibulk == 0);
            if (*ptr == '*')
            {
                ++ ptr;
                
                parseIntRet = GetIntUntilCRLF(ptr, end - ptr, m_multibulk);
                if (parseIntRet == QParseInt::ok)
                {
                    m_state = ParseCmdState::Arglen;
                    ptr += 2; // skip CRLF
                }
                else if (parseIntRet == QParseInt::error)
                {
                    OnError();
                    return 0;
                }
            }
            else
            {
                ptr += _ProcessInlineCmd(ptr, bytes);
            }
                
            break;
                
        case ParseCmdState::Arglen:
            assert(*ptr == '$');
            ++ ptr;
                
            parseIntRet = GetIntUntilCRLF(ptr, end - ptr, m_paramLen);
            if (parseIntRet == QParseInt::ok)
            {
                m_state = ParseCmdState::Arg;
                ptr += 2; // skip CRLF
            }
            else if (parseIntRet == QParseInt::error)
            {
                OnError();
                return 0;
            }
                
            break;
                
        case ParseCmdState::Arg:
        {
            const char* crlf = SearchCRLF(ptr, end - ptr);
                
            if (!crlf)
            {
                USR << "wait crlf for arg";
                break;
            }
                
            if (crlf - ptr != m_paramLen)
            {
                ERR << "param len said " << m_paramLen << ", but actual get " << (crlf - ptr);
                OnError();
                return 0;
            }
                
            m_params.emplace_back(QString(ptr, crlf - ptr));
            if (m_params.size() == static_cast<size_t>(m_multibulk))
            {
                m_state =  ParseCmdState::Ready;
            }
            else
            {
                m_state =  ParseCmdState::Arglen;
            }
                
            ptr = crlf + 2; // skip CRLF
            break;
        }
                
        default:
            break;
    }
    
    if (m_state != ParseCmdState::Ready)
        return static_cast<BODY_LENGTH_T>(ptr - start);

    /// handle packet
    s_pCurrentClient = this;
    
    QString& cmd = m_params[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
    
    QSTORE.SelectDB(m_db);
    
    INF << "client " << GetID() << ", cmd " << m_params[0].c_str();
    
    FeedMonitors(m_params);
    
    const QCommandInfo* info = QCommandTable::GetCommandInfo(m_params[0]);
    // if is multi state,  GetCmdInfo, CheckParamsCount;
    if (IsFlagOn(ClientFlag_multi))
    {
        if (cmd != "multi" &&
            cmd != "exec" &&
            cmd != "watch" &&
            cmd != "unwatch" &&
            cmd != "discard")
        {
            if (!info || !info->CheckParamsCount(static_cast<int>(m_params.size())))
            {
                ERR << "queue failed: cmd " << cmd.c_str() << " has params " << m_params.size();
                ReplyError(info ? QError_param : QError_unknowCmd, &m_reply);
                SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
                FlagExecWrong();
            }
            else
            {
                if (!IsFlagOn(ClientFlag_wrongExec))
                    m_queueCmds.push_back(m_params);
                
                SendPacket("+QUEUED\r\n", 9);
                INF << "queue cmd " << cmd.c_str();
            }
            
            _Reset();
            return static_cast<BODY_LENGTH_T>(ptr - start);
        }
    }
    
    QError err = QError_ok;
    if (QReplication::Instance().GetMasterInfo().state != QReplState_none &&
        !IsFlagOn(ClientFlag_master) &&
        (info->attr & QCommandAttr::QAttr_write))
    {
        ReplyError(err = QError_readonlySlave, &m_reply);
    }
    else
    {
        QSlowLog::Instance().Begin();
        err = QCommandTable::ExecuteCmd(m_params,
                                        info,
                                        IsFlagOn(ClientFlag_master) ? nullptr : &m_reply);
        QSlowLog::Instance().EndAndStat(m_params);
    }
    
    if (!m_reply.IsEmpty())
    {
        SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
    }
    
    if (err == QError_ok && (info->attr & QAttr_write))
    {
        Propogate(m_params);
    }
    
    _Reset();
    
    return static_cast<BODY_LENGTH_T>(ptr - start);
}

QClient*  QClient::Current()
{
    return s_pCurrentClient;
}

QClient::QClient() : m_db(0), m_flag(0), m_name("clientxxx")
{
    SelectDB(0);
    _Reset();
}

bool QClient::SelectDB(int db)
{ 
    if (QSTORE.SelectDB(db) >= 0)
    {
        m_db = db;
        return true;
    }

    return false;
}

void QClient::_Reset()
{
    s_pCurrentClient = 0;

    m_state     = ParseCmdState::Init;
    m_multibulk = 0;
    m_paramLen  = 0;
    m_params.clear();
    m_reply.Clear();
}

// multi
bool QClient::Watch(const QString& key)
{
    INF << "Watch " << key.c_str();
    return m_watchKeys.insert(key).second;
}

void QClient::UnWatch()
{
    m_watchKeys.clear();
}

bool QClient::NotifyDirty(const QString& key)
{
    if (IsFlagOn(ClientFlag_dirty))
    {
        INF << "client is already dirty";
        return false;
    }
    
    if (m_watchKeys.count(key))
    {
        INF << "Dirty client because key " << key.c_str();
        SetFlag(ClientFlag_dirty);
        return true;
    }
    else
    {
        ERR << "BUG: Dirty key is not exist " << key.c_str();
        assert(0);
    }
    
    return false;
}

bool QClient::Exec()
{
    if (IsFlagOn(ClientFlag_wrongExec))
    {
        return false;
    }
    
    if (IsFlagOn(ClientFlag_dirty))
    {
        FormatNullArray(&m_reply);
        return true;
    }
    
    PreFormatMultiBulk(m_queueCmds.size(), &m_reply);
    for (auto it(m_queueCmds.begin());
              it != m_queueCmds.end();
              ++ it)
    {
        INF << "EXEC " << (*it)[0].c_str();
        const QCommandInfo* info = QCommandTable::GetCommandInfo((*it)[0]);
        QError err = QCommandTable::ExecuteCmd(*it, info, &m_reply);
        SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
        _Reset();
        
        // may dirty clients;
        if (err == QError_ok && (info->attr & QAttr_write))
        {
            Propogate(*it);
        }
    }
    
    return true;
}

void QClient::ClearMulti()
{
//    m_watchKeys.clear();
    m_queueCmds.clear();
    ClearFlag(ClientFlag_multi);
    ClearFlag(ClientFlag_dirty);
    ClearFlag(ClientFlag_wrongExec);
}


bool  QClient::WaitFor(const QString& key, const QString* target)
{
    bool  succ = m_waitingKeys.insert(key).second;
    
    if (succ && target)
    {
        if (!m_target.empty())
        {
            ERR << "Wait failed for key " << key << ", because old target " << m_target;
            m_waitingKeys.erase(key);
            return false;
        }
        
        m_target = *target;
    }
    
    return succ;
}


void   QClient::SetSlaveInfo()
{
    m_slaveInfo.reset(new QSlaveInfo());
}


static void Propogate(const std::vector<QString>& params)
{
    ++ QStore::m_dirty;
    QMulti::Instance().NotifyDirty(params[1]);
    if (g_config.appendonly)
        QAOFThreadController::Instance().SaveCommand(params, QSTORE.GetDB());
    
    // replication
    QReplication::Instance().SendToSlaves(params);
}

void  QClient::AddCurrentToMonitor()
{
    s_monitors.insert(std::static_pointer_cast<QClient>(s_pCurrentClient->shared_from_this()));
}

void  QClient::FeedMonitors(const std::vector<QString>& params)
{
    assert(!params.empty());

    if (s_monitors.empty())
        return;

    char buf[512];
    int n = snprintf(buf, sizeof buf, "+[db%d %s:%hu]: \"",
             QSTORE.GetDB(),
             s_pCurrentClient->m_peerAddr.GetIP(),
             s_pCurrentClient->m_peerAddr.GetPort());

    if (n > sizeof buf)
    {
        ERR << "why snprintf return " << n << " bigger than buf size " << sizeof buf;
        n = sizeof buf;
    }
    
    for (const auto& e : params)
    {
        if (sizeof buf > n)
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
