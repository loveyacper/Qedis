#ifndef BERT_LOGGER_H
#define BERT_LOGGER_H

#include <string>
#include <set>
#include <memory>

#include "../Threads/ThreadPool.h"
#include "../AsyncBuffer.h"
#include "MemoryFile.h"
#include "../Timer.h"

enum LogLevel
{
    logINFO     = 0x01 << 0,
    logDEBUG    = 0x01 << 1,
    logWARN     = 0x01 << 2,
    logERROR    = 0x01 << 3,
    logUSR      = 0x01 << 4,
    logALL      = 0xFFFFFFFF,
};

enum LogDest
{
    logConsole  = 0x01 << 0,
    logFILE     = 0x01 << 1,
    logSocket   = 0x01 << 2,  // TODO : LOG SERVER
};

unsigned int ConvertLogLevel(const std::string& level);

class Logger
{
public:

	friend class LogManager;

    bool Init(unsigned int level = logDEBUG,
              unsigned int dest = logConsole,
              const char* pDir  = 0);
    
    void Flush(LogLevel  level);
    bool IsLevelForbid(unsigned int level)  {  return  !(level & level_); };

    Logger&  operator<<(const char* msg);
    Logger&  operator<<(const unsigned char* msg);
    Logger&  operator<<(const std::string& msg);
    Logger&  operator<<(void* );
    Logger&  operator<<(unsigned char a);
    Logger&  operator<<(char a);
    Logger&  operator<<(unsigned short a);
    Logger&  operator<<(short a);
    Logger&  operator<<(unsigned int a);
    Logger&  operator<<(int a);
    Logger&  operator<<(unsigned long a);
    Logger&  operator<<(long a);
    Logger&  operator<<(unsigned long long a);
    Logger&  operator<<(long long a);
    Logger&  operator<<(double a);

    Logger& SetCurLevel(unsigned int level)
    {
        curLevel_ = level;
        return *this;
    }

    bool   Update();

private:
    Logger();
   ~Logger();

    static const size_t MAXLINE_LOG = 2048; // TODO
    char            tmpBuffer_[MAXLINE_LOG];
    std::size_t     pos_;
    
    Time            time_;

    unsigned int    level_;
    std::string     directory_;
    unsigned int    dest_;
    std::string     fileName_;

    unsigned int    curLevel_;

    AsyncBuffer    buffer_;
    OutputMemoryFile file_;
    
    // for optimization
    uint64_t        lastLogSecond_;
    uint64_t        lastLogMSecond_;
    
    std::size_t     _Log(const char* data, std::size_t len);

    bool    _CheckChangeFile();
    const std::string& _MakeFileName();
    bool    _OpenLogFile(const char* name);
    void    _CloseLogFile();
    void    _WriteLog(int level, std::size_t nLen, const char* data);
    void    _Color(unsigned int color);
    void    _Reset();
};



class LogHelper
{
public:
    LogHelper(LogLevel level);
    Logger& operator=(Logger& log);

private:
    LogLevel    level_;
};



class LogManager
{
public:
    static LogManager& Instance();

    ~LogManager();
    
    LogManager(const LogManager& ) = delete;
    void operator=(const LogManager& ) = delete;

    Logger*  CreateLog(unsigned int level = logDEBUG,
                       unsigned int dest = logConsole,
                       const char* pDir  = 0);

    bool    StartLog();
    void    StopLog();
    bool    Update();

    Logger* NullLog()  {  return  &nullLog_;  }

private:
    LogManager();

    Logger  nullLog_;

    std::mutex          logsMutex_;
    std::set<Logger* >  logs_;
    

    class LogThread
    {
    public:
        LogThread() : alive_(false) {}
        
        void  SetAlive() { alive_ = true; }
        bool  IsAlive() const { return alive_; }
        void  Stop() {  alive_ = false; }
        
        void  Run();
    private:
        std::atomic<bool> alive_;
    };
    
    std::shared_ptr<LogThread>  logThread_;
};

extern __thread Logger*  g_log;
extern __thread unsigned g_logLevel;
extern __thread unsigned g_logDest;

#undef INF
#undef DBG
#undef WRN
#undef ERR
#undef USR

#define  LOG_DBG(x)      (LogHelper(logDEBUG))=(x ? x : LogManager::Instance().NullLog())->SetCurLevel(logDEBUG)
#define  LOG_INF(x)      (LogHelper(logINFO))=(x ? x : LogManager::Instance().NullLog())->SetCurLevel(logINFO)
#define  LOG_WRN(x)      (LogHelper(logWARN))=(x ? x : LogManager::Instance().NullLog())->SetCurLevel(logWARN)
#define  LOG_ERR(x)      (LogHelper(logERROR))=(x ? x : LogManager::Instance().NullLog())->SetCurLevel(logERROR)
#define  LOG_USR(x)      (LogHelper(logUSR))=(x ? x : LogManager::Instance().NullLog())->SetCurLevel(logUSR)

#define  DBG      LOG_DBG(g_log)
#define  INF      LOG_INF(g_log)
#define  WRN      LOG_WRN(g_log)
#define  ERR      LOG_ERR(g_log)
#define  USR      LOG_USR(g_log)

#endif

