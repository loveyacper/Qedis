
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <errno.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>

#if defined(__APPLE__)
#include <unistd.h>
#endif

enum LogColor
{
    RED_COLOR    = 1,
    GREEN_COLOR     ,
    YELLOW_COLOR    ,
    NORMAL_COLOR    ,
    BLUE_COLOR      ,
    PURPLE_COLOR    ,
    WHITE_COLOR     ,
    COLOR_MAX       ,
} ;

#include "Logger.h"
#include "../Timer.h"
#include "../Threads/ThreadPool.h"



static const size_t DEFAULT_LOGFILESIZE = 8 * 1024 * 1024;
static const size_t PREFIX_LEVEL_LEN    = 6;
static const size_t PREFIX_TIME_LEN     = 20;

Logger::Logger() : m_buffer(2 * 1024 * 1024),
m_backBytes(0),
m_level(0),
m_dest(0),
m_pMemory(0),
m_offset(DEFAULT_LOGFILESIZE), 
m_file(-1)
{
    _Reset();
    m_fileName.reserve(32);
}

Logger::~Logger()
{
    _CloseLogFile();
}

bool Logger::Init(unsigned int level, unsigned int dest, const char* pDir)
{
    m_level      = level;
    m_dest       = dest;
    m_directory  = pDir ? pDir : ".";

    if (0 == m_level)
    {
        std::cout << "Init log with level 0\n";
        return  true;
    }
  
    if (m_dest & logFILE)
    {
        if (m_directory != ".")
            return _MakeDir(m_directory.c_str());
    }

    return true;
}

bool Logger::_CheckChangeFile()
{
    if (m_file == -1)
        return true;
    
    return m_offset + MAXLINE_LOG >= DEFAULT_LOGFILESIZE;
}

const std::string& Logger::_MakeFileName()
{
    char   name[32];
    Time   now;
    now.FormatTime(name, sizeof(name) - 1);

    m_fileName  = m_directory + "/" + name;
    m_fileName += ".log";

    return m_fileName;
}

bool Logger::_OpenLogFile(const char* name)
{ 
    // CLOSE PREVIOUS LOG FILE PLEASE!
    if (-1 != m_file)
        return false;  

    m_file = ::open(name, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (-1 == m_file)
        return false;

    struct stat st;
    fstat(m_file, &st);
    m_offset  = st.st_size; // for append

    ::ftruncate(m_file, DEFAULT_LOGFILESIZE);
    m_pMemory = (char* )::mmap(0, DEFAULT_LOGFILESIZE, PROT_WRITE, MAP_SHARED, m_file, 0);
    return (char*)-1 != m_pMemory;
}

bool Logger::_CloseLogFile()
{
    if (-1 != m_file)
    {
        ::munmap(m_pMemory, DEFAULT_LOGFILESIZE);
        ::ftruncate(m_file, m_offset);
        ::close(m_file);

        m_pMemory   = 0;
        m_offset    = 0;
        m_file      = -1;
    }

    return true;
}


void Logger::Flush(enum LogLevel level)
{
    if (IsLevelForbid(m_curLevel) ||
        !(level & m_curLevel) ||
         (m_pos <= PREFIX_TIME_LEN + PREFIX_LEVEL_LEN)) 
    {
        _Reset();
        return;
    }

    g_now.Now();
    g_now.FormatTime(m_tmpBuffer, PREFIX_TIME_LEN + 1);

    switch(level)  
    {
    case logINFO:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[INF]:", PREFIX_LEVEL_LEN);
        break;

    case logDEBUG:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[DBG]:", PREFIX_LEVEL_LEN);
        break;

    case logWARN:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[WRN]:", PREFIX_LEVEL_LEN);
        break;

    case logERROR:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[ERR]:", PREFIX_LEVEL_LEN);
        break;

    case logUSR:
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[USR]:", PREFIX_LEVEL_LEN);
        break;

    default:    
        strncpy(m_tmpBuffer + PREFIX_TIME_LEN, "[???]:", PREFIX_LEVEL_LEN);
        break;
    }

    m_tmpBuffer[m_pos ++] = '\n';
    m_tmpBuffer[m_pos] = '\0';

    // Format: level info, length info, log msg
    int logLevel = level;
    if (m_backBytes == 0 &&
        m_buffer.PushDataAt(&logLevel, sizeof logLevel) && 
        m_buffer.PushDataAt(&m_pos, sizeof m_pos, sizeof logLevel) && 
        m_buffer.PushDataAt(m_tmpBuffer, m_pos, sizeof logLevel + sizeof m_pos))
    {
        m_buffer.AdjustWritePtr(sizeof logLevel + sizeof m_pos + m_pos);
    }
    else
    {
        ScopeMutex   lock(m_backBufLock);
        m_backBuf.PushData(&logLevel, sizeof logLevel);
        m_backBuf.PushData(&m_pos, sizeof m_pos);
        m_backBuf.PushData(m_tmpBuffer, m_pos);

        m_backBytes = m_backBuf.ReadableSize();

        _Color(NORMAL_COLOR);
        std::cerr << "push bytes to back buffer " << m_pos << std::endl;
        std::cerr << "push content to back buffer " << m_tmpBuffer << std::endl;
    }

    _Reset();
}

void Logger::_Color(unsigned int color)
{
    static const char* colorstrings[COLOR_MAX] = {
        "",
        "\033[1;31;40m",
        "\033[1;32;40m",
        "\033[1;33;40m",
        "\033[0m",
        "\033[1;34;40m",
        "\033[1;35;40m",
        "\033[1;37;40m",
    };

    fprintf(stdout, "%s", colorstrings[color]);
}

bool Logger::_MakeDir(const char* pDir)
{
    if (pDir && 0 != mkdir(pDir, 0755))
    {
        if (EEXIST != errno)
            return false;
    }
    return true;
}

Logger&  Logger::operator<< (const char* msg)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }

    const size_t len = strlen(msg);
    if (m_pos + len >= MAXLINE_LOG)  
    {
        return *this;
    }

    memcpy(m_tmpBuffer + m_pos, msg, len);
    m_pos += len;

    return  *this;
}

Logger&  Logger::operator<< (const unsigned char* msg)
{
    return operator<<(reinterpret_cast<const char*>(msg));
}

Logger&  Logger::operator<< (void* ptr)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    
    if (m_pos + 18 < MAXLINE_LOG)
    {  
        unsigned long ptrValue = (unsigned long)ptr;
        int nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%#018lx", ptrValue);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (unsigned char a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }

    if (m_pos + 3 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hhd", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (char a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 3 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hhu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned short a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 5 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (short a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 5 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hd", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned int a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 10 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%u", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (int a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 10 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%d", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%lu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%ld", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%llu", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long long a)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }
    if (m_pos + 20 < MAXLINE_LOG)
    {
        int  nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%lld", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}


void   Logger::_Reset()
{
    m_curLevel = 0;
    m_pos  = PREFIX_LEVEL_LEN + PREFIX_TIME_LEN ;
}

bool Logger::Update()
{
    if (m_buffer.IsEmpty())
    {
        bool  hasLog = false;
        if (m_backBytes > 0 && m_backBufLock.TryLock())
        {
            hasLog = true;

            while (!m_backBuf.IsEmpty())
            {
            int level = 0;
            m_backBuf.PeekData(&level, sizeof level);

            int nLen = 0;
            m_backBuf.PeekData(&nLen, sizeof nLen);

            assert (nLen && level);
            _WriteLog(level, nLen, m_backBuf.ReadAddr());

            m_backBuf.AdjustReadPtr(nLen);
            }

            m_backBytes = 0;
            m_backBufLock.Unlock();
        }

        return  hasLog;
    }

    while (!m_buffer.IsEmpty())
    {
        int level = 0;
        m_buffer.PeekDataAt(&level, sizeof level);
        m_buffer.AdjustReadPtr(sizeof level);

        size_t nLen = 0;
        m_buffer.PeekDataAt(&nLen, sizeof nLen);
        m_buffer.AdjustReadPtr(sizeof nLen);

        BufferSequence  bf;
        m_buffer.GetDatum(bf, nLen);
        assert (nLen == bf.TotalBytes());

        AttachedBuffer   content(bf);
        _WriteLog(level, nLen, content.ReadAddr());

        m_buffer.AdjustReadPtr(nLen);
    }

    return true;
}

void Logger::_WriteLog(int level, size_t nLen, const char* data)
{
    assert (nLen > 0 && data);
    
    if (m_dest & logConsole)
    {
        switch (level)
        {
        case logINFO:
            _Color(GREEN_COLOR);
            break;

        case logDEBUG:
            _Color(WHITE_COLOR);
            break;

        case logWARN:
            _Color(YELLOW_COLOR);
            break;

        case logERROR:
            _Color(RED_COLOR);
            break;

        case logUSR:
            _Color(PURPLE_COLOR);
            break;

        default:
            _Color(RED_COLOR);
            break;
        }

        fprintf(stdout, "%.*s", static_cast<int>(nLen), data);
        _Color(NORMAL_COLOR);
    }

    if (m_dest & logFILE)
    {
        while (_CheckChangeFile())
        {
            if (!_CloseLogFile() || !_OpenLogFile(_MakeFileName().c_str()))
            {   //OOPS!!! IMPOSSIBLE!
                break;
            }
        }

        if (m_pMemory && m_pMemory != (char*)-1)
        {
            ::memcpy(m_pMemory + m_offset, data, nLen);
            m_offset += nLen;
        }
    }
}



LogHelper::LogHelper(LogLevel level) : m_level(level)    
{
}
    
Logger& LogHelper::operator=(Logger& log) 
{
    log.Flush(m_level);
    return  log;
}



LogManager& LogManager::Instance()
{
    static LogManager mgr;
    return mgr;
}

LogManager::LogManager()
{
    m_nullLog.Init(0);
    m_logThread.Reset(new LogThread);
}

LogManager::~LogManager()
{
    LOG_SET::iterator it(m_logs.begin());
    for ( ; it != m_logs.end(); ++ it)
    {
        (*it)->Update();
        delete *it;    
    }
}

Logger*  LogManager::CreateLog(unsigned int level ,
               unsigned int dest ,
               const char* pDir)
{
    assert (!m_logThread->IsAlive() && "You can not create log after log-thread running");

    Logger*  pLog(new Logger);
    if (!pLog->Init(level, dest, pDir))
    {
        delete pLog;
        return &m_nullLog;
    }
    else
    {
        m_logs.insert(pLog);
    }

    return pLog;
}

bool LogManager::StartLog()
{
    assert (!m_logThread->IsAlive());
    std::cout << "start log thread\n";
    return ThreadPool::Instance().ExecuteTask(m_logThread);
}

void LogManager::StopLog()
{
    m_logThread->Stop();
}

bool LogManager::Update()
{
    bool busy = false;

    for (LOG_SET::iterator it(m_logs.begin());
        it != m_logs.end();
        ++ it)
    {
        if ((*it)->Update() && !busy)
        {
            busy = true;
        }
    }

    return  busy;
}


void  LogManager::LogThread::Run()
{
    m_alive = true;
    while (IsAlive())
    {
        if (!LogManager::Instance().Update())
            Thread::YieldCPU();
    }
}
