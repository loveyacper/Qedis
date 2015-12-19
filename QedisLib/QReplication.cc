
#include <unistd.h>
#include <iostream> // the child process use stdout for log

#include "QClient.h"
#include "QConfig.h"
#include "QCommon.h"
#include "QDB.h"
#include "QReplication.h"

#include "QAOF.h" // FOR save changes commands
#include "Server.h"


QReplication& QReplication::Instance()
{
    static QReplication  rep;
    return rep;
}

QReplication::QReplication()
{
}

bool  QReplication::IsBgsaving() const
{
    return  bgsaving_;
}

void  QReplication::SetBgsaving(bool b)
{
    bgsaving_ = b;
}

void  QReplication::AddSlave(QClient* cli)
{
    slaves_.push_back(std::static_pointer_cast<QClient>(cli->shared_from_this()));
}

bool  QReplication::HasAnyWaitingBgsave() const
{
    for (const auto& c : slaves_)
    {
        auto cli = c.lock();
        if (cli && cli->GetSlaveInfo()->state == QSlaveState_wait_bgsave_start)
        {
            return true;
        }
    }
    
    return false;
}

void   QReplication::OnRdbSaveDone()
{
    SetBgsaving(false);
    
    InputMemoryFile  rdb;
    
    // send rdb to slaves that  wait rdb end, set state
    for (auto& wptr : slaves_)
    {
        auto cli = wptr.lock();
        if (!cli)
            continue;
        
        if (cli->GetSlaveInfo()->state == QSlaveState_wait_bgsave_end)
        {
            cli->GetSlaveInfo()->state = QSlaveState_online;
            
            
            if (!rdb.IsOpen() && !rdb.Open(g_config.rdbfullname.c_str()))
            {
                ERR << "can not open rdb when replication\n";
                return;  // fatal error;
            }
            
            std::size_t   size = std::size_t(-1);
            const char* data = rdb.Read(size);
            
            // $file_len + filedata
            char tmp[32];
            int n =  snprintf(tmp, sizeof tmp - 1, "$%ld\r\n", (long)size);
            
            cli->SendPacket(tmp, n);
            cli->SendPacket(data, size);
            cli->SendPacket(buffer_.ReadAddr(), buffer_.ReadableSize());
            
            INF << "Send to slave rdb " << size << ", buffer " << buffer_.ReadableSize();
        }
    }
    
    buffer_.Clear();
}


void   QReplication::TryBgsave()
{
    if (IsBgsaving())
        return;
    
    if (!HasAnyWaitingBgsave())
        return;
    
    int ret = fork();
    if (ret == 0)
    {
        {
            QDBSaver  qdb;
            qdb.Save(g_config.rdbfullname.c_str());
            std::cerr << "QReplication save rdb done, exiting child\n";
        }
        _exit(0);
    }
    else if (ret == -1)
    {
        ERR << "QReplication save rdb FATAL ERROR";
        _OnStartBgsave(false);
    }
    else
    {
        INF << "QReplication save rdb START";
        g_qdbPid = ret;
        _OnStartBgsave(true);
    }
}


void   QReplication::_OnStartBgsave(bool succ)
{
    buffer_.Clear();
    SetBgsaving(succ);
    
    for (auto& c : slaves_)
    {
        auto cli = c.lock();
        
        if (!cli)
            continue;
    
        if (cli->GetSlaveInfo()->state == QSlaveState_wait_bgsave_start)
        {
            if (succ)
            {
                INF << "_OnStartBgsave set cli wait bgsave end " << cli->GetName();
                cli->GetSlaveInfo()->state = QSlaveState_wait_bgsave_end;
            }
            else
            {
                cli->OnError(); // release slave
            }
        }
    }
}


void  QReplication::SaveChanges(const std::vector<QString>& params)
{
    if (!IsBgsaving())
        return;
    
    SaveCommand(params, buffer_);
}



void   QReplication::SendToSlaves(const std::vector<QString>& params)
{
    //StackBuffer<8 * 1024>  sb; TODO stack buffer
    
    UnboundedBuffer   ub;
    
    for (const auto& wptr : slaves_)
    {
        auto cli = wptr.lock();
        if (!cli || cli->GetSlaveInfo()->state != QSlaveState_online)
            continue;
        
        if (ub.IsEmpty())
            SaveCommand(params, ub);
        
        cli->SendPacket(ub.ReadAddr(), ub.ReadableSize());
    }
}

void   QReplication::Cron()
{
    static unsigned  pingCron = 0;
    
    if (pingCron ++ % 50 == 0)
    {
        for (const auto& wptr : slaves_)
        {
            auto cli = wptr.lock();
            if (!cli || cli->GetSlaveInfo()->state != QSlaveState_online)
                continue;
        
            cli->SendPacket("PING\r\n", 6);
        }
    }
    
    if (!masterInfo_.addr.Empty())
    {
        switch (masterInfo_.state)
        {
            case QReplState_none:
            {
                INF << "Try connect to master " << masterInfo_.addr.GetIP();
                Server::Instance()->TCPConnect(masterInfo_.addr, true);
                masterInfo_.state = QReplState_connecting;
            }
                break;
                
            case QReplState_connected:
            {
                auto master = master_.lock();
                if (!master)
                {
                    masterInfo_.state = QReplState_none;
                    INF << "Master is down from connected to none";
                }
                else
                {
                    master->SendPacket("SYNC\r\n", 6);
                    INF << "Request SYNC";
                    
                    rdb_.Open(slaveRdbFile, false);
                    masterInfo_.rdbRecved = 0;
                    masterInfo_.rdbSize   = std::size_t(-1);
                    masterInfo_.state = QReplState_wait_rdb;
                }
            }
                break;
                
            case QReplState_wait_rdb:
            {
            }
                break;
                
            default:
                break;
        }
    }
    else
    {
        if (masterInfo_.state != QReplState_none)
        {
            auto master = master_.lock();
            if (master)
            {
                SocketAddr  peer;
                Socket::GetPeerAddr(master->GetSocket(), peer);
                INF << master->GetName() << " disconnect with Master " << peer.GetIP();
                
                master->SetReconn(false);
                master->OnError();
            }
            
            masterInfo_.state = QReplState_none;
        }
    }
}


void   QReplication::SaveTmpRdb(const char* data, std::size_t len)
{
    rdb_.Write(data, len);
    masterInfo_.rdbRecved += len;
    
    if (masterInfo_.rdbRecved == masterInfo_.rdbSize)
    {
        INF << "Rdb recv complete, bytes " << masterInfo_.rdbSize;
        
        QSTORE.ResetDb();
        
        QDBLoader  loader;
        loader.Load(slaveRdbFile);
        masterInfo_.state = QReplState_online;
    }
}
