#include "QClient.h"
#include "QStore.h"
#include "Log/Logger.h"
#include "QCommand.h"
#include "QMulti.h"

// *3  CR LF
// $4  CR LF
// sadd CR LF
// $5  CR LF
// myset CR LF
// $3   CR LF
// 345 CR LF

// temp
enum QParseInt
{
    QParseInt_ok,
    QParseInt_waitCrlf,
    QParseInt_error,
};

static inline QParseInt  GetIntUntilCRLF(const char*& ptr, std::size_t nBytes, int& val)
{
    assert (ptr && nBytes);

    val = 0;
    std::size_t i = 0;
    for (i = 0; i < nBytes; ++ i)
    {
        if (isdigit(ptr[i]))
        {
            val *= 10;
            val += ptr[i] - '0';
        }
        else
        {
            if (i == 0 || ptr[i] != '\r' || (i+1 < nBytes && ptr[i+1] != '\n'))
                return QParseInt_error;

            if (i+1 == nBytes)
                return QParseInt_waitCrlf;

            break;
        }
    }

    ptr += i;
    return QParseInt_ok;
}


QClient*  QClient::s_pCurrentClient = 0;
    
HEAD_LENGTH_T QClient::_HandleHead(AttachedBuffer& buf, BODY_LENGTH_T* bodyLen)
{
    const size_t   bytes = buf.ReadableSize();
    const char* const start = buf.ReadAddr();

    const char* ptr  = start;

    QParseInt parseIntRet = QParseInt_ok;
    bool  parseBody = false;
    while (static_cast<size_t>(ptr - start) < bytes && !parseBody)
    {

    switch (m_state)
    {
    case InitState:
        assert (m_multibulk == 0);
        m_stat.Begin();
        if (*ptr != '*')
        {
            ERR << "InitState: expect *, wrong char " << (int)*ptr;
            OnError();
            assert (false);
            return 0;
        }

        m_state = ProcessMultiBulkState;
        ++ ptr;
        break;

    case ProcessMultiBulkState:
        parseIntRet = GetIntUntilCRLF(ptr, start + bytes - ptr, m_multibulk);
        if (parseIntRet == QParseInt_waitCrlf)
        {
            return static_cast<int>(ptr - start);
        }
        else if (parseIntRet == QParseInt_error)
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
            assert (false);
            return 0;
        }
        m_state = ProcessDollarState;
        break;

    case ProcessDollarState:
        if (*ptr == '$')
        {
            ++ ptr;
            m_state = ProcessArglenState;
        }
        else
        {
            ERR << "ProcessDollarState: expect $, wrong char " << (int)*ptr;
            OnError();
            assert (false);
            return 0;
        }
        break;

    case ProcessArglenState:
        parseIntRet = GetIntUntilCRLF(ptr, start + bytes - ptr, m_paramLen);
        if (parseIntRet == QParseInt_waitCrlf)
        {
            return static_cast<int>(ptr - start);
        }
        else if (parseIntRet == QParseInt_error)
        {
            OnError();
            assert (false);
            return 0;
        }

        assert (ptr[0] == 13 && ptr[1] == 10);
        ptr += 2;

        m_state = ProcessArgState;
        if (m_paramLen < 0 || m_paramLen > 1024 * 1024)
        {
            ERR << "Got wrong argLen " << m_paramLen;
            OnError();
            assert (false);
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


void QClient::_HandlePacket(AttachedBuffer& buf)
{
    s_pCurrentClient = this;

    const std::size_t bytes = buf.ReadableSize();
    const char* const start = buf.ReadAddr();

    const char* ptr  = start;
    const char* crlf = 0;

    if (m_state == ProcessArgState)
    {
        crlf = SearchCRLF(start, bytes);

        if (!crlf) 
        {
            ERR << "Why can not find crlf?";
            OnError();
            assert (false);
        }
        else
            assert (crlf[0] == 13 && crlf[1] == 10);

        if (crlf - start != m_paramLen)
        {
            ERR << "param len said " << m_paramLen << ", but actual get " << (crlf - start);
            OnError();
            return ;
        }

        m_params.emplace_back(QString(ptr, crlf - start));
        if (m_params.size() == static_cast<size_t>(m_multibulk))
        {
            m_state = ReadyState;
        }
        else
        {
            m_state = ProcessDollarState;
            return;
        }
    }

    assert (m_state = ReadyState);

    QString& cmd = m_params[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    bool succ = (QSTORE.SelectDB(m_db) >= 0);
    assert (succ);
    m_stat.End(PARSE_STATE);

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
                ReplyError(info ? QError_param : QError_unknowCmd, m_reply);
                SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
                FlagExecWrong();
            }
            else
            {
                m_queueCmds.push_back(m_params);
                SendPacket("+QUEUED\r\n", 9);
                INF << "queue cmd " << cmd.c_str();
            }

            _Reset();
            return;
        }
    }
    // return;
    
    m_stat.Begin();
    QError err = QCommandTable::ExecuteCmd(m_params, info, m_reply);
    m_stat.End(PROCESS_STATE);

    if (!m_reply.IsEmpty())
    {
        m_stat.Begin();
        SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
        m_stat.End(SEND_STATE);
    }
    
    if (err == QError_ok && (info->attr & QAttr_write))
    {
        QMulti::Instance().NotifyDirty(m_params[1]);
    }
    
    _Reset();
}


QClient*  QClient::Current()
{
    return s_pCurrentClient;
}

QClient::QClient() : m_db(0), m_flag(0)
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

    m_state     = InitState;
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
        FormatNullArray(m_reply);
        return true;
    }
    
    PreFormatMultiBulk(m_queueCmds.size(), m_reply);
    for (auto it(m_queueCmds.begin());
         it != m_queueCmds.end();
         ++ it)
    {
        INF << "EXEC " << (*it)[0].c_str();
        const QCommandInfo* info = QCommandTable::GetCommandInfo((*it)[0]);
        QError err = QCommandTable::ExecuteCmd(*it, info, m_reply);
        SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
        _Reset();
        
        // may dirty clients;
        if (err == QError_ok && (info->attr & QAttr_write))
        {
            QMulti::Instance().NotifyDirty((*it)[1]);
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


