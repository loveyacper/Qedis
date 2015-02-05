
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <errno.h>


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


static const size_t DEFAULT_LOGFILESIZE = 32 * 1024 * 1024;
static const size_t PREFIX_LEVEL_LEN    = 6;
static const size_t PREFIX_TIME_LEN     = 24;

Logger::Logger() : m_level(0),
                   m_dest(0)
{
    m_thread = Thread::GetCurrentThreadId();
    m_lastLogMSecond = m_lastLogSecond = -1;
    _Reset();
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
        if (m_directory == "." ||
            MemoryFile::MakeDir(m_directory.c_str()))
        {
            _OpenLogFile(_MakeFileName().c_str());
            return true;
        }
        
        return false;
    }

    if (!(m_dest & logConsole))
    {
        std::cerr << "log has no output, but loglevel is " << level << std::endl;
        return false;
    }
            
    return true;
}

bool Logger::_CheckChangeFile()
{
    if (!m_file.IsOpen())
        return true;
    
    return m_file.Offset() + MAXLINE_LOG > DEFAULT_LOGFILESIZE;
}

const std::string& Logger::_MakeFileName()
{
    char   name[32];
    Time   now;
    size_t len = now.FormatTime(name);
    name[len] = '\0';

    m_fileName  = m_directory + "/" + name;
    m_fileName += ".log";

    return m_fileName;
}

bool Logger::_OpenLogFile(const char* name)
{ 
    return  m_file.Open(name, true);
}

void Logger::_CloseLogFile()
{
    return m_file.Close();
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

    assert (m_thread == Thread::GetCurrentThreadId());
    
    m_time.Now();
#if 0
    m_time.FormatTime(m_tmpBuffer);
#else
    auto seconds = m_time.MilliSeconds() / 1000;
    if (seconds != m_lastLogSecond)
    {
        m_time.FormatTime(m_tmpBuffer);
        m_lastLogSecond = seconds;
    }
    else
    {
        auto msec = m_time.MilliSeconds() % 1000;
        if (msec != m_lastLogMSecond)
        {
            snprintf(m_tmpBuffer + 20, 4, "%03d", static_cast<int>(msec));
            m_tmpBuffer[23] = ']';
            m_lastLogMSecond = msec;
        }
    }
#endif

    switch(level)
    {
    case logINFO:
        memcpy(m_tmpBuffer + PREFIX_TIME_LEN, "[INF]:", PREFIX_LEVEL_LEN);
        break;

    case logDEBUG:
        memcpy(m_tmpBuffer + PREFIX_TIME_LEN, "[DBG]:", PREFIX_LEVEL_LEN);
        break;

    case logWARN:
        memcpy(m_tmpBuffer + PREFIX_TIME_LEN, "[WRN]:", PREFIX_LEVEL_LEN);
        break;

    case logERROR:
        memcpy(m_tmpBuffer + PREFIX_TIME_LEN, "[ERR]:", PREFIX_LEVEL_LEN);
        break;

    case logUSR:
        memcpy(m_tmpBuffer + PREFIX_TIME_LEN, "[USR]:", PREFIX_LEVEL_LEN);
        break;

    default:    
        memcpy(m_tmpBuffer + PREFIX_TIME_LEN, "[???]:", PREFIX_LEVEL_LEN);
        break;
    }

    m_tmpBuffer[m_pos ++] = '\n';
    m_tmpBuffer[m_pos] = '\0';

    // Format: level info, length info, log msg
    int logLevel = level;

    BufferSequence  contents;
    contents.count = 3;

    contents.buffers[0].iov_base = &logLevel;
    contents.buffers[0].iov_len  = sizeof logLevel;
    contents.buffers[1].iov_base = &m_pos;
    contents.buffers[1].iov_len  = sizeof m_pos;
    contents.buffers[2].iov_base = m_tmpBuffer;
    contents.buffers[2].iov_len  = m_pos;

    m_buffer.AsyncWrite(contents);

    _Reset();
}

void Logger::_Color(unsigned int color)
{
#if defined(__gnu_linux__)
    const char* colorstrings[COLOR_MAX] = {
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
#endif
}

Logger&  Logger::operator<< (const char* msg)
{
    if (IsLevelForbid(m_curLevel))
    {
        return *this;
    }

    const auto len = strlen(msg);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%#018lx", ptrValue);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hhd", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hhu", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hu", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%hd", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%u", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%d", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%lu", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%ld", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%llu", a);
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
        auto nbytes = snprintf(m_tmpBuffer + m_pos, MAXLINE_LOG - m_pos, "%lld", a);
        if (nbytes > 0) m_pos += nbytes;
    }

    return  *this;
}

bool Logger::Update()
{
    BufferSequence  data;
    m_buffer.ProcessBuffer(data);
    
    AttachedBuffer  abf(data);
    auto nWritten = _Log(abf.ReadAddr(), abf.ReadableSize());
    m_buffer.Skip(nWritten);
    
    return nWritten != 0;
}

void   Logger::_Reset()
{
    m_curLevel = 0;
    m_pos  = PREFIX_LEVEL_LEN + PREFIX_TIME_LEN ;
}

size_t  Logger::_Log(const char* data, size_t dataLen)
{
    const auto minLogSize = sizeof(int) + sizeof(size_t);

    size_t   nOffset = 0;
    while (nOffset + minLogSize < dataLen)
    {
        int  level = *(int*)(data + nOffset);
        size_t len = *(size_t* )(data + nOffset + sizeof(int));
        if (dataLen < nOffset + minLogSize + len)
        {
            std::cerr << "_WriteLog skip 0!!!\n ";
            break;
        }

        _WriteLog(level, len, data + nOffset + minLogSize);
        nOffset += minLogSize + len;
    }

    return  nOffset;
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
            _CloseLogFile();
            if (!_OpenLogFile(_MakeFileName().c_str()))
            {   //OOPS!!! IMPOSSIBLE!
                break;
            }
        }

        assert (m_file.IsOpen());
        m_file.Write(data, nLen);
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
    m_logThread.reset(new LogThread);
}

LogManager::~LogManager()
{
    for (auto log : m_logs)
    {
        int i = 0;
        while (log->Update())
        {
            std::cerr << i++ << " when exit update log ptr " << (void*)log << std::endl;
        }
        delete log;
    }
}

Logger*  LogManager::CreateLog(unsigned int level ,
               unsigned int dest ,
               const char* pDir)
{
    Logger*  pLog(new Logger);
    if (!pLog->Init(level, dest, pDir))
    {
        delete pLog;
        return &m_nullLog;
    }
    else
    {
        std::lock_guard<std::mutex>  guard(m_logsMutex);
        m_logs.insert(pLog);
    }

    return pLog;
}

bool LogManager::StartLog()
{
    std::cout << "start log thread\n";

    assert (!m_logThread->IsAlive());
    m_logThread->SetAlive();

    return ThreadPool::Instance().ExecuteTask(m_logThread);
}

void LogManager::StopLog()
{
    std::cout << "stop log thread\n";
    m_logThread->Stop();
}

bool LogManager::Update()
{
    bool busy = false;
    
    std::lock_guard<std::mutex>  guard(m_logsMutex);
  
    for (auto log : m_logs)
    {
        if (log->Update() && !busy)
        {
            busy = true;
        }
    }

    return  busy;
}


void  LogManager::LogThread::Run()
{
    while (IsAlive())
    {
        if (!LogManager::Instance().Update())
            Thread::YieldCPU();
    }
}
