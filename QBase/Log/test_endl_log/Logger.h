#ifndef BERT_LOGGER_H
#define BERT_LOGGER_H

#include <string>
#include <set>

#include "./Buffer.h"

class Logger
{
public:
    static Logger& endl(Logger& log)
    {
        if (!log.IsLevelForbid(log.m_curLevel)) {
            log << "\n";
            log.Flush(LogLevel(log.m_curLevel));
            log.Update();
        }
        return log;
    }

    Logger&  operator<<(Logger& (*fp)(Logger& ))
    {
        fp(*this);
        return *this;
    }

    enum LogLevel
    {
        logINFO     = 0x01 << 0,
        logERROR    = 0x01 << 1,
        logALL      = 0xFFFFFFFF,
    };

    enum LogDest
    {
        logConsole  = 0x01 << 0,
        logFILE     = 0x01 << 1,
        logSocket   = 0x01 << 2,  // TODO : LOG SERVER
    };

    bool Init(unsigned int level = logINFO,
              unsigned int dest = logConsole,
              const char* pDir  = 0);
    
    void Flush(LogLevel  level);
    bool IsLevelForbid(unsigned int level)  {  return  !(level & m_level); };

    Logger&  operator<<(const char* msg);
    Logger&  operator<<(const unsigned char* msg);
    Logger&  operator<<(void* );
    Logger&  operator<<(unsigned int a);
    Logger&  operator<<(int a);

    Logger& SetCurLevel(unsigned int level)
    {
        m_curLevel = level;
        return *this;
    }

    bool   Update();

    Logger();
   ~Logger();

private:

    static const int MAXLINE_LOG = 2048; // TODO
    char            m_tmpBuffer[MAXLINE_LOG];
    int             m_pos;
    Buffer          m_buffer;

    unsigned int    m_level;
    std::string     m_directory;
    unsigned int    m_dest;
    std::string     m_fileName;

    unsigned int    m_curLevel;

    char*           m_pMemory;
    int             m_offset;
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


#undef LOG_INF
#undef LOG_ERR

#define  LOG_INF(pLog)      (pLog).SetCurLevel(Logger::logINFO)
#define  LOG_ERR(pLog)      (pLog).SetCurLevel(Logger::logERROR)



#endif
