#ifndef BERT_QREPLICATION_H
#define BERT_QREPLICATION_H

#include <list>
#include <memory>
#include "UnboundedBuffer.h"

enum QSlaveState
{
    QSlaveState_none,
    QSlaveState_wait_bgsave_start, // 有非sync的bgsave进行 要等待
    QSlaveState_wait_bgsave_end,   // sync bgsave正在进行
    //QSlaveState_send_rdb, // 这个slave在接受rdb文件
    QSlaveState_online,
    
};

struct  QMasterInfo
{
    //SocketAddr  masterAddr;
};

struct QSlaveInfo
{
    QSlaveState  state;
    
    QSlaveInfo() : state(QSlaveState_none)
    {
    }
};

class QClient;

class QReplication
{
public:
    static QReplication& Instance();
    
    bool   IsBgsaving() const;
    void   SetBgsaving(bool b);
    
    void   AddSlave(QClient* cli);
    
    void   TryBgsave();
    bool   StartBgsave();
    void   OnStartBgsave();
    
    bool   HasAnyWaitingBgsave() const;

    void   OnRdbSaveDone();
    // RDB期间缓存变化
    void   SaveChanges(const std::vector<QString>& params);
    
    void   SendToSlaves(const std::vector<QString>& params);
    
    void   Cron();
    
private:
    QReplication();
    void    _OnStartBgsave(bool succ);
    
    bool    m_bgsaving;
    
    std::list<std::weak_ptr<QClient> >  m_slaves;
    
    UnboundedBuffer  m_buffer; // 存rdb期间，缓冲变化
    // SOCKET::SENDPACKET是一定成功的，所以，当文件发送完毕，立即将此buffer SEND
};


#endif
