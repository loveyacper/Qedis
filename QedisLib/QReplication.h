#ifndef BERT_QREPLICATION_H
#define BERT_QREPLICATION_H

#include <list>
#include <memory>
#include "UnboundedBuffer.h"
#include "Socket.h"
#include "Log/MemoryFile.h"

namespace qedis
{

// master side
enum QSlaveState
{
    QSlaveState_none,
    QSlaveState_wait_bgsave_start, // 有非sync的bgsave进行 要等待
    QSlaveState_wait_bgsave_end,   // sync bgsave正在进行
    //QSlaveState_send_rdb, // 这个slave在接受rdb文件
    QSlaveState_online,
};

struct QSlaveInfo
{
    QSlaveState  state;
    
    QSlaveInfo() : state(QSlaveState_none)
    {
    }
};

// slave side
enum QReplState
{
    QReplState_none,
    QReplState_connecting,
    QReplState_connected,
    QReplState_wait_rdb,
    QReplState_online,
};

struct QMasterInfo
{
    SocketAddr  addr;
    
    QReplState  state;
    
    // For recv rdb
    std::size_t rdbSize;
    std::size_t rdbRecved;
    
    QMasterInfo()
    {
        state   = QReplState_none;
        rdbSize = std::size_t(-1);
        rdbRecved = 0;
    }
};

//tmp filename
const char*  const slaveRdbFile = "slave.rdb";

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
    
    void   SetMaster(const std::shared_ptr<QClient>&  cli) { master_ = cli; }
    QMasterInfo& GetMasterInfo() { return masterInfo_; }
    
    void   SaveTmpRdb(const char* data, std::size_t len);
    
private:
    QReplication();
    void    _OnStartBgsave(bool succ);
    
    bool    bgsaving_;
    
    std::list<std::weak_ptr<QClient> >  slaves_;
    
    UnboundedBuffer  buffer_; // 存rdb期间，缓冲变化
    // SOCKET::SENDPACKET是一定成功的，所以，当文件发送完毕，立即将此buffer SEND

    QMasterInfo             masterInfo_;
    std::weak_ptr<QClient>  master_;

    OutputMemoryFile        rdb_;
};

}

#endif
