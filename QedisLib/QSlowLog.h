#ifndef BERT_QSLOWLOG_H
#define BERT_QSLOWLOG_H

#include <vector>

#include "Log/Logger.h"
#include "QString.h"

class  QSlowLog
{
public:
    static QSlowLog& Instance();

    void Begin();
    void EndAndStat(const std::vector<QString>& cmds);
    
    void SetThreshold(unsigned int );

private:
    QSlowLog();
    ~QSlowLog();
    
    unsigned int m_threshold;
    long long    m_beginUs;
    Logger*      m_logger;
};

#endif

