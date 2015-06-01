#include <sys/time.h>
#include <fstream>
#include <sstream>

#include "QSlowlog.h"


QSlowLog& QSlowLog::Instance()
{
    static QSlowLog slog;

    return slog;
}

QSlowLog::QSlowLog() : m_threshold(0), m_logger(nullptr)
{
}

QSlowLog::~QSlowLog()
{
}

void QSlowLog::SetThreshold(unsigned int v)
{
    m_threshold = v;
}

void QSlowLog::Begin()
{
    if (!m_threshold)
        return;

    timeval  begin;
    gettimeofday(&begin, 0);
    m_beginUs = begin.tv_sec * 1000000 + begin.tv_usec;
}



void QSlowLog::EndAndStat(const std::vector<QString>& cmds)
{
    if (!m_threshold)
        return;
    
    timeval  end;
    gettimeofday(&end, 0);
    unsigned int used = end.tv_sec * 1000000 + end.tv_usec - m_beginUs;
    
    if (used >= m_threshold)
    {
        if (m_logger == nullptr)
            m_logger = LogManager::Instance().CreateLog(logALL, logFILE, "slowlog.qedis");
        
        LOG_INF(m_logger) << "+++++++++++";
        LOG_INF(m_logger) << "Used:(us) " << used;
        
        for (const auto& param : cmds)
        {
            LOG_INF(m_logger) << param;
        }
        
        LOG_INF(m_logger) << "-----------";
    }
}
