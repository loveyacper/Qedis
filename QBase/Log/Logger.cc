
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <errno.h>
#include <sys/stat.h>


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


static const size_t DEFAULT_LOGFILESIZE = 32 * 1024 * 1024;
static const size_t PREFIX_LEVEL_LEN    = 6;
static const size_t PREFIX_TIME_LEN     = 24;

unsigned int ConvertLogLevel(const std::string& level)
{
    unsigned int l = logALL;
    
    if (level == "debug")
    {
        ;
    }
    else if (level == "verbose")
    {
        l &= ~logDEBUG;
    }
    else if (level == "notice")
    {
        l &= ~logDEBUG;
        l &= ~logINFO;
    }
    else if (level == "warning")
    {
        l &= ~logDEBUG;
        l &= ~logINFO;
        l &= ~logWARN; // redis warning is my error
    }
    else if (level == "none")
    {
        l = 0;
    }
    
    return l;
}

static bool MakeDir(const char* pDir)
{
    if (mkdir(pDir, 0755) != 0)
    {
        if (EEXIST != errno)
            return false;
    }
    
    return true;
}

__thread Logger*  g_log = nullptr;
__thread unsigned g_logLevel;
__thread unsigned g_logDest;

Logger::Logger() : level_(0),
                   dest_(0)
{
    lastLogMSecond_ = lastLogSecond_ = -1;
    _Reset();
}

Logger::~Logger()
{
    _CloseLogFile();
}

bool Logger::Init(unsigned int level, unsigned int dest, const char* pDir)
{
    level_      = level;
    dest_       = dest;
    directory_  = pDir ? pDir : ".";

    if (0 == level_)
    {
        return  true;
    }
  
    if (dest_ & logFILE)
    {
        if (directory_ == "." ||
            MakeDir(directory_.c_str()))
        {
            _OpenLogFile(_MakeFileName().c_str());
            return true;
        }
        
        return false;
    }

    if (!(dest_ & logConsole))
    {
        std::cerr << "log has no output, but loglevel is " << level << std::endl;
        return false;
    }
            
    return true;
}

bool Logger::_CheckChangeFile()
{
    if (!file_.IsOpen())
        return true;
    
    return file_.Size() + MAXLINE_LOG > DEFAULT_LOGFILESIZE;
}

const std::string& Logger::_MakeFileName()
{
    char   name[32];
    Time   now;
    size_t len = now.FormatTime(name);
    name[len] = '\0';

    fileName_  = directory_ + "/" + name;
    fileName_ += ".log";

    return fileName_;
}

bool Logger::_OpenLogFile(const char* name)
{ 
    return  file_.Open(name, true);
}

void Logger::_CloseLogFile()
{
    return file_.Close();
}


void Logger::Flush(enum LogLevel level)
{
    if (IsLevelForbid(curLevel_) ||
        !(level & curLevel_) ||
         (pos_ <= PREFIX_TIME_LEN + PREFIX_LEVEL_LEN)) 
    {
        _Reset();
        return;
    }
    
    time_.Now();
#if 0
    time_.FormatTime(tmpBuffer_);
#else
    auto seconds = time_.MilliSeconds() / 1000;
    if (seconds != lastLogSecond_)
    {
        time_.FormatTime(tmpBuffer_);
        lastLogSecond_ = seconds;
    }
    else
    {
        auto msec = time_.MilliSeconds() % 1000;
        if (msec != lastLogMSecond_)
        {
            snprintf(tmpBuffer_ + 20, 4, "%03d", static_cast<int>(msec));
            tmpBuffer_[23] = ']';
            lastLogMSecond_ = msec;
        }
    }
#endif

    switch(level)
    {
    case logINFO:
        memcpy(tmpBuffer_ + PREFIX_TIME_LEN, "[INF]:", PREFIX_LEVEL_LEN);
        break;

    case logDEBUG:
        memcpy(tmpBuffer_ + PREFIX_TIME_LEN, "[DBG]:", PREFIX_LEVEL_LEN);
        break;

    case logWARN:
        memcpy(tmpBuffer_ + PREFIX_TIME_LEN, "[WRN]:", PREFIX_LEVEL_LEN);
        break;

    case logERROR:
        memcpy(tmpBuffer_ + PREFIX_TIME_LEN, "[ERR]:", PREFIX_LEVEL_LEN);
        break;

    case logUSR:
        memcpy(tmpBuffer_ + PREFIX_TIME_LEN, "[USR]:", PREFIX_LEVEL_LEN);
        break;

    default:    
        memcpy(tmpBuffer_ + PREFIX_TIME_LEN, "[???]:", PREFIX_LEVEL_LEN);
        break;
    }

    tmpBuffer_[pos_ ++] = '\n';
    tmpBuffer_[pos_] = '\0';

    // Format: level info, length info, log msg
    int logLevel = level;

    BufferSequence  contents;
    contents.count = 3;

    contents.buffers[0].iov_base = &logLevel;
    contents.buffers[0].iov_len  = sizeof logLevel;
    contents.buffers[1].iov_base = &pos_;
    contents.buffers[1].iov_len  = sizeof pos_;
    contents.buffers[2].iov_base = tmpBuffer_;
    contents.buffers[2].iov_len  = pos_;

    buffer_.Write(contents);

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
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }

    const auto len = strlen(msg);
    if (pos_ + len >= MAXLINE_LOG)  
    {
        return *this;
    }

    memcpy(tmpBuffer_ + pos_, msg, len);
    pos_ += len;

    return  *this;
}

Logger&  Logger::operator<< (const unsigned char* msg)
{
    return operator<<(reinterpret_cast<const char*>(msg));
}

Logger&  Logger::operator<< (const std::string& msg)
{
    return operator<<(msg.c_str());
}

Logger&  Logger::operator<< (void* ptr)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    
    if (pos_ + 18 < MAXLINE_LOG)
    {  
        unsigned long ptrValue = (unsigned long)ptr;
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%#018lx", ptrValue);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (unsigned char a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }

    if (pos_ + 3 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%hhd", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}


Logger&  Logger::operator<< (char a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 3 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%hhu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned short a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 5 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%hu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (short a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 5 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%hd", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned int a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 10 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%u", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (int a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 10 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%d", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 20 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%lu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 20 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%ld", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (unsigned long long a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 20 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%llu", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (long long a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 20 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%lld", a);
        if (nbytes > 0) pos_ += nbytes;
    }

    return  *this;
}

Logger&  Logger::operator<< (double a)
{
    if (IsLevelForbid(curLevel_))
    {
        return *this;
    }
    if (pos_ + 20 < MAXLINE_LOG)
    {
        auto nbytes = snprintf(tmpBuffer_ + pos_, MAXLINE_LOG - pos_, "%.6g", a);
        if (nbytes > 0) pos_ += nbytes;
    }
    
    return  *this;
}

bool Logger::Update()
{
    BufferSequence  data;
    buffer_.ProcessBuffer(data);
    
    AttachedBuffer  abf(data);
    auto nWritten = _Log(abf.ReadAddr(), abf.ReadableSize());
    buffer_.Skip(nWritten);
    
    return nWritten != 0;
}

void   Logger::_Reset()
{
    curLevel_ = 0;
    pos_  = PREFIX_LEVEL_LEN + PREFIX_TIME_LEN ;
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
    
    if (dest_ & logConsole)
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

    if (dest_ & logFILE)
    {
        while (_CheckChangeFile())
        {
            _CloseLogFile();
            if (!_OpenLogFile(_MakeFileName().c_str()))
            {   //OOPS!!! IMPOSSIBLE!
                break;
            }
        }

        assert (file_.IsOpen());
        file_.Write(data, nLen);
    }
}



LogHelper::LogHelper(LogLevel level) : level_(level)    
{
}
    
Logger& LogHelper::operator=(Logger& log) 
{
    log.Flush(level_);
    return  log;
}



LogManager& LogManager::Instance()
{
    static LogManager mgr;
    return mgr;
}

LogManager::LogManager()
{
    nullLog_.Init(0);
    logThread_.reset(new LogThread);
}

LogManager::~LogManager()
{
    for (auto log : logs_)
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
        return &nullLog_;
    }
    else
    {
        std::lock_guard<std::mutex>  guard(logsMutex_);
        logs_.insert(pLog);
    }

    return pLog;
}

bool LogManager::StartLog()
{
    std::cout << "start log thread\n";

    assert (!logThread_->IsAlive());
    logThread_->SetAlive();

    ThreadPool::Instance().ExecuteTask(std::bind(&LogThread::Run, logThread_.get()));
    return true;
}

void LogManager::StopLog()
{
    std::cout << "stop log thread\n";
    logThread_->Stop();
}

bool LogManager::Update()
{
    bool busy = false;
    
    std::lock_guard<std::mutex>  guard(logsMutex_);
  
    for (auto log : logs_)
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
            std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}
