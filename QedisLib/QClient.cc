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

void QClient::_ProcessInlineCmd(const char* buf, size_t bytes, BODY_LENGTH_T* bodyLen)
{
    if (bytes < 2)
        return;
    
    size_t cursor = bytes - 1;
    for (; cursor > 0; -- cursor)
    {
        if (buf[cursor] == '\n' && buf[cursor-1] == '\r')
        {
            *bodyLen = cursor + 1;
            m_state = ParseCmdState::Ready;
            break;
        }
    }
    
    if (m_state == ParseCmdState::Ready)
    {
        QString   param;
        for (size_t i = 0; i + 2 < cursor; ++ i)
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
        
        m_params.emplace_back(std::move(param));
    }
}

HEAD_LENGTH_T QClient::_HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen)
{
    const size_t   bytes = buf.ReadableSize();
    const char* const start = buf.ReadAddr();

    const char* ptr  = start;

    QParseInt parseIntRet = QParseInt::ok;
    bool  parseBody = false;
    while (static_cast<size_t>(ptr - start) < bytes && !parseBody)
    {

    switch (m_state)
    {
    case ParseCmdState::Init:
        assert (m_multibulk == 0);
        if (*ptr != '*')
        {
            INF << "Try process inline cmd first char " << (int)*ptr;
            _ProcessInlineCmd(ptr, bytes, bodyLen);
            return 0;
        }

        m_state = ParseCmdState::MultiBulk;
        ++ ptr;
        break;

    case ParseCmdState::MultiBulk:
        parseIntRet = GetIntUntilCRLF(ptr, start + bytes - ptr, m_multibulk);
        if (parseIntRet == QParseInt::waitCrlf)
        {
            return static_cast<int>(ptr - start);
        }
        else if (parseIntRet == QParseInt::error)
        {
            OnError();
            return 0;
        }

        assert (ptr[0] == 13 && ptr[1] == 10);
        ptr += 2;
            
        if (m_multibulk < 0 || m_multibulk > 1024)
        {
            ERR << "Abnormal m_multibulk " << m_multibulk;
            OnError();
            return 0;
        }
        
        m_state = ParseCmdState::Dollar;
        break;

    case ParseCmdState::Dollar:
        if (*ptr == '$')
        {
            ++ ptr;
            m_state = ParseCmdState::Arglen;
        }
        else
        {
            ERR << "ProcessDollarState: expect $, wrong char " << (int)*ptr;
            OnError();
            return 0;
        }
        break;

    case ParseCmdState::Arglen:
        parseIntRet = GetIntUntilCRLF(ptr, start + bytes - ptr, m_paramLen);
        if (parseIntRet == QParseInt::waitCrlf)
        {
            return static_cast<int>(ptr - start);
        }
        else if (parseIntRet == QParseInt::error)
        {
            OnError();
            ERR << "ProcessArglenState: parseIntError";
            return 0;
        }

        assert (ptr[0] == 13 && ptr[1] == 10);
        ptr += 2;

        m_state = ParseCmdState::Arg;
        if (m_paramLen < 0 || m_paramLen > 1024 * 1024)
        {
            ERR << "Got wrong argLen " << m_paramLen;
            OnError();
            return 0;
        }

        *bodyLen = m_paramLen + 2; // crlf
        parseBody = true;
        break;

    default:
        assert (!!!"Wrong state when handle head");
        break;
    }

    }

    return static_cast<int>(ptr - start);
}


static void Propogate(const std::vector<QString>& params);

void QClient::_HandlePacket(AttachedBuffer& buf)
{
    s_pCurrentClient = this;

    const std::size_t bytes = buf.ReadableSize();
    const char* const start = buf.ReadAddr();

    const char* ptr  = start;
    const char* crlf = 0;

    if (m_state ==  ParseCmdState::Arg)
    {
        crlf = SearchCRLF(start, bytes);

        if (!crlf) 
        {
            ERR << "Why can not find crlf?";
            OnError();
            return;
        }

        if (crlf - start != m_paramLen)
        {
            ERR << "param len said " << m_paramLen << ", but actual get " << (crlf - start);
            OnError();
            return ;
        }

        m_params.emplace_back(QString(ptr, crlf - start));
        if (m_params.size() == static_cast<size_t>(m_multibulk))
        {
            m_state =  ParseCmdState::Ready;
        }
        else
        {
            m_state =  ParseCmdState::Dollar;
            return;
        }
    }

    assert (m_state == ParseCmdState::Ready);

    QString& cmd = m_params[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    QSTORE.SelectDB(m_db);

    INF << "client " << GetID() << ", cmd " << m_params[0].c_str();
    
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
            return;
        }
    }
    
    QSlowLog::Instance().Begin();
    QError err = QCommandTable::ExecuteCmd(m_params, info, &m_reply);
    QSlowLog::Instance().EndAndStat(m_params);

    if (!m_reply.IsEmpty())
    {
        SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
    }
    
    if (err == QError_ok && (info->attr & QAttr_write))
    {
        Propogate(m_params);
    }
    
    _Reset();
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

static void Propogate(const std::vector<QString>& params)
{
    ++ QStore::m_dirty;
    QMulti::Instance().NotifyDirty(params[1]);
    if (g_config.appendonly)
        QAOFThreadController::Instance().SaveCommand(params, QSTORE.GetDB());
    
}
