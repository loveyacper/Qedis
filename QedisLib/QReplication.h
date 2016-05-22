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
    unsigned short listenPort; // slave listening port
    
    QSlaveInfo() : state(QSlaveState_none), listenPort(0)
    {
    }
};

// slave side
enum QReplState
{
    QReplState_none,
    QReplState_connecting,
    QReplState_connected,
    QReplState_wait_auth,
    QReplState_wait_rdb,
    QReplState_online,
};

struct QMasterInfo
{
    SocketAddr  addr;
    QReplState  state;
    time_t downSince;
    
    // For recv rdb
    std::size_t rdbSize;
    std::size_t rdbRecved;
    
    QMasterInfo()
    {
        state   = QReplState_none;
        downSince = 0;
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
    
    QReplication(const QReplication& ) = delete;
    void operator= (const QReplication& ) = delete;
    
    void Cron();
    
    // master side
    bool IsBgsaving() const;
    bool HasAnyWaitingBgsave() const;
    void AddSlave(QClient* cli);
    void TryBgsave();
    bool StartBgsave();
    void OnStartBgsave();
    void OnRdbSaveDone();
    void SendToSlaves(const std::vector<QString>& params);
    
    // slave side
    void SaveTmpRdb(const char* data, std::size_t len);
    void SetMaster(const std::shared_ptr<QClient>&  cli);
    void SetMasterState(QReplState s);
    void SetMasterAddr(const char* ip, unsigned short port);
    void SetRdbSize(std::size_t s);
    QReplState GetMasterState() const;
    SocketAddr GetMasterAddr() const;
    std::size_t GetRdbSize() const;
    
    // info command
    void OnInfoCommand(UnboundedBuffer& res);

private:
    QReplication();
    void _OnStartBgsave(bool succ);
    
    // master side
    bool bgsaving_;
    UnboundedBuffer buffer_;
    std::list<std::weak_ptr<QClient> > slaves_;

    //slave side
    QMasterInfo masterInfo_;
    std::weak_ptr<QClient> master_;
    OutputMemoryFile rdb_;
};

}

#define QREPL  qedis::QReplication::Instance()

#endif
