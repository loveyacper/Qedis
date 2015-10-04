
#include <unistd.h>

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
    return  m_bgsaving;
}

void  QReplication::SetBgsaving(bool b)
{
    m_bgsaving = b;
}

void  QReplication::AddSlave(QClient* cli)
{
    m_slaves.push_back(std::static_pointer_cast<QClient>(cli->shared_from_this()));
}

bool  QReplication::HasAnyWaitingBgsave() const
{
    for (const auto& c : m_slaves)
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
    for (auto& wptr : m_slaves)
    {
        auto cli = wptr.lock();
        if (!cli)
            continue;
        
        if (cli->GetSlaveInfo()->state == QSlaveState_wait_bgsave_end)
        {
            cli->GetSlaveInfo()->state = QSlaveState_online;
            
            
            if (!rdb.IsOpen() && !rdb.Open(g_config.rdbfilename.c_str()))
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
            cli->SendPacket(m_buffer.ReadAddr(), m_buffer.ReadableSize());
            
            INF << "Send to slave rdb " << size << ", buffer " << m_buffer.ReadableSize();
        }
    }
    
    m_buffer.Clear();
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
            qdb.Save(g_config.rdbfilename.c_str());
            INF << "QReplication save rdb done, exiting child";
        }
        exit(0);
    }
    else if (ret == -1)
    {
        ERR << "QReplication save rdb FATAL ERROR";
        _OnStartBgsave(false); // 设置slave状态
        return;
    }
    else
    {
        INF << "QReplication save rdb START";
        g_qdbPid = ret;
        SetBgsaving(true);
        _OnStartBgsave(true); // 设置slave状态
    }
}


void   QReplication::_OnStartBgsave(bool succ)
{
    m_buffer.Clear();
    
    for (auto& c : m_slaves)
    {
        auto cli = c.lock();
        
        if (!cli)
            continue;
    
        if (cli->GetSlaveInfo()->state == QSlaveState_wait_bgsave_start)
        {
            if (succ)
            {
                INF << "_OnStartBgsave Set cli state " << cli->GetName();
                cli->GetSlaveInfo()->state = QSlaveState_wait_bgsave_end;
            }
            else
            {
                cli->OnError(); // release slave?
            }
        }
    }
}


void  QReplication::SaveChanges(const std::vector<QString>& params)
{
    if (!IsBgsaving())
        return;
    
    SaveCommand(params, m_buffer);
}



void   QReplication::SendToSlaves(const std::vector<QString>& params)
{
    //StackBuffer<8 * 1024>  sb; TODO stack buffer
    
    UnboundedBuffer   ub;
    
    for (const auto& wptr : m_slaves)
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
        for (const auto& wptr : m_slaves)
        {
            auto cli = wptr.lock();
            if (!cli || cli->GetSlaveInfo()->state != QSlaveState_online)
                continue;
        
            cli->SendPacket("PING\r\n", 6);
        }
    }
    
    if (!m_masterInfo.addr.Empty())
    {
        switch (m_masterInfo.state)
        {
            case QReplState_none:
            {
                INF << "Try connect to master " << m_masterInfo.addr.GetIP();
                Server::Instance()->TCPConnect(m_masterInfo.addr, true);
                m_masterInfo.state = QReplState_connecting;
            }
                break;
                
            case QReplState_connected:
            {
                auto master = m_master.lock();
                if (!master)
                {
                    m_masterInfo.state = QReplState_none;
                    INF << "Master is down from connected to none";
                }
                else
                {
                    master->SendPacket("SYNC\r\n", 6);
                    INF << "Request SYNC";
                    
                    m_rdb.Open(slaveRdbFile, false);
                    m_masterInfo.rdbRecved = 0;
                    m_masterInfo.rdbSize   = std::size_t(-1);
                    m_masterInfo.state = QReplState_wait_rdb;
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
        if (m_masterInfo.state != QReplState_none)
        {
            auto master = m_master.lock();
            if (master)
            {
                SocketAddr  peer;
                Socket::GetPeerAddr(master->GetSocket(), peer);
                INF << master->GetName() << " disconnect with Master " << peer.GetIP();
                
                master->SetReconn(false);
                master->OnError();
            }
            
            m_masterInfo.state = QReplState_none;
        }
    }
}


void   QReplication::SaveTmpRdb(const char* data, std::size_t len)
{
    m_rdb.Write(data, len);
    m_masterInfo.rdbRecved += len;
    
    if (m_masterInfo.rdbRecved == m_masterInfo.rdbSize)
    {
        INF << "Rdb recv complete, bytes " << m_masterInfo.rdbSize;
        
        QSTORE.ResetDb();
        
        QDBLoader  loader;
        loader.Load(slaveRdbFile);
        m_masterInfo.state = QReplState_online;
    }
}
