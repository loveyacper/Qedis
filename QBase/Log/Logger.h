#ifndef BERT_LOGGER_H
#define BERT_LOGGER_H

#include <string>
#include <set>

#include "../Threads/Thread.h"
#include "../Buffer.h"
#include "../UnboundedBuffer.h"

class Logger
{
public:
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

	friend class LogManager;

    bool Init(unsigned int level = logDEBUG,
              unsigned int dest = logConsole,
              const char* pDir  = 0);
    
    void Flush(LogLevel  level);
    bool IsLevelForbid(unsigned int level)  {  return  !(level & m_level); };

    Logger&  operator<<(const char* msg);
    Logger&  operator<<(const unsigned char* msg);
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

    Logger& SetCurLevel(unsigned int level)
    {
        m_curLevel = level;
        return *this;
    }

    bool   Update();

private:
    //NONCOPYABLE(Logger);

    Logger();
   ~Logger();

    static const int MAXLINE_LOG = 2048; // TODO
    char            m_tmpBuffer[MAXLINE_LOG];
    std::size_t     m_pos;
    Buffer          m_buffer;

    Mutex           m_backBufLock;
    std::size_t     m_backBytes;
    UnboundedBuffer m_backBuf;

    unsigned int    m_level;
    std::string     m_directory;
    unsigned int    m_dest;
    std::string     m_fileName;

    unsigned int    m_curLevel;

    char*           m_pMemory;
    std::size_t     m_offset;
    int             m_file;

    bool    _CheckChangeFile();
    const std::string& _MakeFileName();
    bool    _OpenLogFile(const char* name);
    bool    _CloseLogFile();
    void    _WriteLog(int level, int nLen, const char* data);
    void    _Color(unsigned int color);
    void    _Reset();

    static bool _MakeDir(const char* pDir);
};



class LogHelper
{
public:
    LogHelper(Logger::LogLevel level);
    Logger& operator=(Logger& log);

private:
    Logger::LogLevel    m_level;
};



class LogManager
{
public:
    static LogManager& Instance();

    ~LogManager();

    Logger*  CreateLog(unsigned int level = Logger::logDEBUG,
                       unsigned int dest = Logger::logConsole,
                       const char* pDir  = 0);

    bool    StartLog();
    void    StopLog();
    bool    Update();

    Logger* NullLog()  {  return  &m_nullLog;  }

private:
  //  NONCOPYABLE(LogManager);

    LogManager();

    Logger  m_nullLog;

    typedef std::set<Logger* >  LOG_SET;
    LOG_SET m_logs;
    

    class LogThread : public Runnable
    {
    public:
        LogThread() : m_alive(false) {}
        
        bool  IsAlive() const { return m_alive; }
        void  Stop() {  m_alive = false; }
        
        virtual void Run();
    private:
        volatile bool m_alive;
    };
    
    SharedPtr<LogThread>  m_logThread;
};



#undef INF
#undef DBG
#undef WRN
#undef ERR
#undef CRI
#undef USR

#define  LOG_INF(pLog)      (LogHelper(Logger::logINFO))=(pLog)->SetCurLevel(Logger::logINFO)
#define  LOG_DBG(pLog)      (LogHelper(Logger::logDEBUG))=(pLog)->SetCurLevel(Logger::logDEBUG)
#define  LOG_WRN(pLog)      (LogHelper(Logger::logWARN))=(pLog)->SetCurLevel(Logger::logWARN)
#define  LOG_ERR(pLog)      (LogHelper(Logger::logERROR))=(pLog)->SetCurLevel(Logger::logERROR)
#define  LOG_USR(pLog)      (LogHelper(Logger::logUSR))=(pLog)->SetCurLevel(Logger::logUSR)


// Ugly debug codes, only for test
#ifdef  DEBUG_BERT_SDK
    extern  Logger*  g_sdkLog;
    extern  Mutex    g_sdkMutex;
    
    #define LOCK_SDK_LOG    g_sdkMutex.Lock();
    #define UNLOCK_SDK_LOG  g_sdkMutex.Unlock();
    #define DEBUG_SDK       DBG(g_sdkLog ? g_sdkLog : LogManager::Instance().NullLog())


#define  INF      (LogHelper(Logger::logINFO))=(g_sdkLog)->SetCurLevel(Logger::logINFO)
#define  DBG      (LogHelper(Logger::logDEBUG))=(g_sdkLog)->SetCurLevel(Logger::logDEBUG)
#define  WRN      (LogHelper(Logger::logWARN))=(g_sdkLog)->SetCurLevel(Logger::logWARN)
#define  ERR      (LogHelper(Logger::logERROR))=(g_sdkLog)->SetCurLevel(Logger::logERROR)
#define  USR      (LogHelper(Logger::logUSR))=(g_sdkLog)->SetCurLevel(Logger::logUSR)

#else
    #define LOCK_SDK_LOG
    #define UNLOCK_SDK_LOG
    #define DEBUG_SDK       DBG(LogManager::Instance().NullLog())


#define  INF      (LogHelper(Logger::logINFO))=(LogManager::Instance().NullLog())->SetCurLevel(Logger::logINFO)
#define  DBG      (LogHelper(Logger::logDEBUG))=(LogManager::Instance().NullLog())->SetCurLevel(Logger::logDEBUG)
#define  WRN      (LogHelper(Logger::logWARN))=(LogManager::Instance().NullLog())->SetCurLevel(Logger::logWARN)
#define  ERR      (LogHelper(Logger::logERROR))=(LogManager::Instance().NullLog())->SetCurLevel(Logger::logERROR)
#define  USR      (LogHelper(Logger::logUSR))=(LogManager::Instance().NullLog())->SetCurLevel(Logger::logUSR)

#endif


#endif
