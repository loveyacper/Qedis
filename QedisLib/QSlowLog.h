#ifndef BERT_QSLOWLOG_H
#define BERT_QSLOWLOG_H

#include <vector>
#include <deque>

#include "QString.h"

class Logger;

namespace qedis
{

struct SlowLogItem
{
    unsigned used;
    std::vector<QString> cmds;
    
    SlowLogItem() : used(0)
    {
    }
    
    SlowLogItem(SlowLogItem&& item) : used(item.used), cmds(std::move(item.cmds))
    {
    }
};

class  QSlowLog
{
public:
    static QSlowLog& Instance();
    
    QSlowLog(const QSlowLog& ) = delete;
    void operator= (const QSlowLog& ) = delete;

    void Begin();
    void EndAndStat(const std::vector<QString>& cmds);
    
    void SetThreshold(unsigned int );
    void SetLogLimit(std::size_t maxCount);
    
    void ClearLogs() { logs_.clear(); }
    std::size_t GetLogsCount() const { return logs_.size(); }
    const std::deque<SlowLogItem>& GetLogs() const { return logs_; }

private:
    QSlowLog();
    ~QSlowLog();
    
    unsigned int threshold_;
    long long    beginUs_;
    Logger*      logger_;
    
    std::size_t  logMaxCount_;
    std::deque<SlowLogItem> logs_;
    
};
    
}

#endif

