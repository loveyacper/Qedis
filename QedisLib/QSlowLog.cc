#include <sys/time.h>
#include <fstream>
#include <sstream>

#include "QSlowLog.h"

namespace qedis
{

QSlowLog& QSlowLog::Instance()
{
    static QSlowLog slog;

    return slog;
}

QSlowLog::QSlowLog() : threshold_(0), logger_(nullptr)
{
}

QSlowLog::~QSlowLog()
{
}

void QSlowLog::SetThreshold(unsigned int v)
{
    threshold_ = v;
}

void QSlowLog::SetLogLimit(std::size_t maxCount)
{
    logMaxCount_ = maxCount;
}

void QSlowLog::Begin()
{
    if (!threshold_)
        return;

    timeval  begin;
    gettimeofday(&begin, 0);
    beginUs_ = begin.tv_sec * 1000000 + begin.tv_usec;
}



void QSlowLog::EndAndStat(const std::vector<QString>& cmds)
{
    if (!threshold_)
        return;
    
    timeval  end;
    gettimeofday(&end, 0);
    auto used = end.tv_sec * 1000000 + end.tv_usec - beginUs_;
    
    if (used >= threshold_)
    {
        if (logger_ == nullptr)
            logger_ = LogManager::Instance().CreateLog(logALL, logFILE, "slowlog.qedis");
        
        LOG_INF(logger_) << "+ Used:(us) " << used;
        
        for (const auto& param : cmds)
        {
            LOG_INF(logger_) << param;
        }
        
        if (cmds[0] == "slowlog")
            return;
        
        SlowLogItem item;
        item.used = static_cast<unsigned>(used);
        item.cmds = cmds;
        
        logs_.emplace_front(std::move(item));
        if (logs_.size() > logMaxCount_)
            logs_.pop_back();
    }
}

}