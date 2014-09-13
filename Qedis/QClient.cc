#include "QClient.h"
#include "QStore.h"
#include "Log/Logger.h"
#include "QCommand.h"

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

static inline QParseInt  GetIntUntilCRLF(const char*& ptr, int nBytes, int& val)
{
    assert (ptr && nBytes);

    val = 0;
    int i = 0;
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
    const int   bytes = buf.ReadableSize();
    const char* const start = buf.ReadAddr();

    const char* ptr  = start;
    const char* crlf = 0;

    QParseInt parseIntRet = QParseInt_ok;
    bool  parseBody = false;
    while (ptr - start < bytes && !parseBody)
    {

    switch (m_state)
    {
    case InitState:
        assert (m_multibulk == 0);
        m_stat.Begin();
        if (*ptr != '*')
        {
            LOG_ERR(g_logger) << "InitState: expect *, wrong char " << (int)*ptr;
            OnError();
            assert (false);
            return 0;
        }

        m_state = ProcessMultiBulkState;
        ++ ptr;
        break;

    case ProcessMultiBulkState:
#if 0
        if (bytes - static_cast<int>(ptr - start) > 2)
        {
            crlf = SearchCRLF(ptr, bytes - static_cast<int>(ptr - start));
        }

        if (!crlf) 
            return static_cast<int>(ptr - start);
        else
            assert (crlf[0] == 13 && crlf[1] == 10);

        if (!Strtol(ptr, static_cast<int>(crlf - ptr), &m_multibulk))
        {
            LOG_ERR(g_logger) << "Can not get args number";
            OnError();
            assert (false);
            return 0;
        }
        ptr = crlf + 2;
#else
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
#endif
            
        INF << "Got multibulk " << m_multibulk;
        if (m_multibulk < 0 || m_multibulk > 1024)
        {
            LOG_ERR(g_logger) << "Abnormal m_multibulk " << m_multibulk;
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
            LOG_ERR(g_logger) << "ProcessDollarState: expect $, wrong char " << (int)*ptr;
            OnError();
            assert (false);
            return 0;
        }
        break;

    case ProcessArglenState:
#if 0
        if (bytes - static_cast<int>(ptr - start) > 2)
        {
            crlf = SearchCRLF(ptr, bytes - static_cast<int>(ptr - start));
        }

        if (!crlf) 
        {
            LOG_INF(g_logger) << "ProcessArglenState can not find crlf, break";
            return 0;
        }
        else
        {
            assert (crlf[0] == 13 && crlf[1] == 10);
        }

        if (!Strtol(ptr, static_cast<int>(crlf - ptr), &m_paramLen))
        {
            LOG_ERR(g_logger) << "Can not get arg len";
            OnError();
            assert (false);
            return 0;
        }
            
        ptr = crlf + 2;
#else
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
#endif
        m_state = ProcessArgState;
        if (m_paramLen < 0 || m_paramLen > 1024 * 1024)
        {
            LOG_ERR(g_logger) << "Got wrong argLen " << m_paramLen;
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

    const int bytes = buf.ReadableSize();
    const char* const start = buf.ReadAddr();

    const char* ptr  = start;
    const char* crlf = 0;

    if (m_state == ProcessArgState)
    {
        crlf = SearchCRLF(start, bytes);

        if (!crlf) 
        {
            LOG_ERR(g_logger) << "Why can not find crlf?";
            OnError();
            assert (false);
        }
        else
            assert (crlf[0] == 13 && crlf[1] == 10);

        if (crlf - start != m_paramLen)
        {
            LOG_ERR(g_logger) << "param len said " << m_paramLen << ", but actual get " << (crlf - start);
            OnError();
            assert (false);
            return ;
        }

        m_params.push_back(QString(ptr, crlf - start));
        if (m_params.size() == m_multibulk)
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
#if 0
    for (std::vector<std::string>::const_iterator it (m_params.begin());
            it != m_params.end();
            ++ it)
        CRI << (*it).c_str();
#endif

    QString& cmd = m_params[0];
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    bool succ = (QSTORE.SelectDB(m_db) >= 0);
    assert (succ);

    m_stat.End(PARSE_STATE);

    m_stat.Begin();
    QError err = QCommandTable::Instance().ExecuteCmd(m_params, m_reply);
    (void)err;
    m_stat.End(PROCESS_STATE);

    if (!m_reply.IsEmpty())
    {
        m_stat.Begin();
        SendPacket(m_reply.ReadAddr(), m_reply.ReadableSize());
        m_stat.End(SEND_STATE);
    }
    _Reset();
}

QClient::QClient() : m_db(0)
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

