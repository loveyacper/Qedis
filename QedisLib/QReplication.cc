
#include <unistd.h>
#include <iostream> // the child process use stdout for log
#include <sstream>

#include "QClient.h"
#include "QConfig.h"
#include "QCommon.h"
#include "QDB.h"
#include "QReplication.h"

#include "QAOF.h"
#include "Server.h"


namespace qedis
{

QReplication& QReplication::Instance()
{
    static QReplication  rep;
    return rep;
}

QReplication::QReplication()
{
}

bool QReplication::IsBgsaving() const
{
    return  bgsaving_;
}

void QReplication::AddSlave(qedis::QClient* cli)
{
    slaves_.push_back(std::static_pointer_cast<QClient>(cli->shared_from_this()));
}

bool QReplication::HasAnyWaitingBgsave() const
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

void QReplication::OnRdbSaveDone()
{
    bgsaving_ = false;
    
    InputMemoryFile  rdb;
    
    // send rdb to slaves that wait rdb end, set state
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
            
            std::size_t size = std::size_t(-1);
            const char* data = rdb.Read(size);
            
            // $file_len + filedata
            char tmp[32];
            int n =  snprintf(tmp, sizeof tmp - 1, "$%ld\r\n", (long)size);
            
            cli->SendPacket(tmp, n);
            cli->SendPacket(data, size);
            cli->SendPacket(buffer_);
            
            INF << "Send to slave rdb " << size << ", buffer " << buffer_.ReadableSize();
        }
    }
    
    buffer_.Clear();
}


void QReplication::TryBgsave()
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


void QReplication::_OnStartBgsave(bool succ)
{
    buffer_.Clear();
    bgsaving_ = succ;
    
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

void QReplication::SendToSlaves(const std::vector<QString>& params)
{
    if (IsBgsaving())
    {
        // 在执行rdb期间，缓存变化
        SaveCommand(params, buffer_);
        return;
    }
    
    UnboundedBuffer   ub;
    
    for (const auto& wptr : slaves_)
    {
        auto cli = wptr.lock();
        if (!cli || cli->GetSlaveInfo()->state != QSlaveState_online)
            continue;
        
        if (ub.IsEmpty())
            SaveCommand(params, ub);
        
        cli->SendPacket(ub);
    }
}

void QReplication::Cron()
{
    static unsigned pingCron = 0;
    
    if (pingCron ++ % 50 == 0)
    {
        for (auto it = slaves_.begin(); it != slaves_.end(); )
        {
            auto cli = it->lock();
            if (!cli)
            {
                it = slaves_.erase(it);
            }
            else
            {
                ++ it;

                if (cli->GetSlaveInfo()->state == QSlaveState_online)
                    cli->SendPacket("PING\r\n", 6);
            }
        }
    }
    
    if (!masterInfo_.addr.Empty())
    {
        switch (masterInfo_.state)
        {
            case QReplState_none:
            {
                INF << "Try connect to master " << masterInfo_.addr.GetIP();
                Server::Instance()->TCPConnect(masterInfo_.addr, [&]() {
                    QREPL.SetMasterState(QReplState_none);
                });

                masterInfo_.state = QReplState_connecting;
            }
                break;
                
            case QReplState_connected:
            {
                auto master = master_.lock();
                if (!master)
                {
                    masterInfo_.state = QReplState_none;
                    masterInfo_.downSince = ::time(nullptr);
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
                
                master->SetOnDisconnect();
                master->OnError();
            }
            
            masterInfo_.state = QReplState_none;
        }
    }
}


void QReplication::SaveTmpRdb(const char* data, std::size_t len)
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
    
void QReplication::SetMaster(const std::shared_ptr<QClient>&  cli)
{
    master_ = cli;
}

void QReplication::SetMasterState(QReplState s)
{
    masterInfo_.state = s;
}

QReplState QReplication::GetMasterState() const
{
    return masterInfo_.state;
}

SocketAddr QReplication::GetMasterAddr() const
{
    return masterInfo_.addr;
}

void QReplication::SetMasterAddr(const char* ip, unsigned short port)
{
    if (ip)
        masterInfo_.addr.Init(ip, port);
    else
        masterInfo_.addr.Clear();
}
    
void QReplication::SetRdbSize(std::size_t s)
{
    masterInfo_.rdbSize = s;
}

std::size_t QReplication::GetRdbSize() const
{
    return masterInfo_.rdbSize;
}

    
QError replconf(const std::vector<QString>& params, UnboundedBuffer* reply)
{
    if (params.size() % 2 == 0)
    {
        ReplyError(QError_syntax, reply);
        return QError_syntax;
    }
    
    for (size_t i = 1; i < params.size(); i += 2)
    {
        if (strncasecmp(params[i].c_str(), "listening-port", 14) == 0)
        {
            long port;
            if (!TryStr2Long(params[i + 1].c_str(), params[i + 1].size(), port))
            {
                ReplyError(QError_param, reply);
                return QError_param;
            }
        
            auto info = QClient::Current()->GetSlaveInfo();
            if (!info)
            {
                QClient::Current()->SetSlaveInfo();
                info = QClient::Current()->GetSlaveInfo();
                QREPL.AddSlave(QClient::Current());
            }
            info->listenPort = static_cast<unsigned short>(port);
        }
        else
        {
            if (reply)
            {
                reply->PushData("-ERR:Unrecognized REPLCONF option:",
                                sizeof "-ERR:Unrecognized REPLCONF option:" - 1);
                reply->PushData(params[i].data(), params[i].size());
            }
            
            return QError_syntax;
        }
    }
    
    FormatOK(reply);
    return QError_ok;
}
    
    
void QReplication::OnInfoCommand(UnboundedBuffer& res)
{
    const char* slaveState[] = { "none",
        "wait_bgsave",
        "wait_bgsave",
        //"send_bulk", // qedis does not have send bulk state
        "online",
        
    };
    
    std::ostringstream oss;
    int index = 0;
    for (const auto& c : slaves_)
    {
        auto cli = c.lock();
        if (cli)
        {
            oss << "slave" << index << ":";
            index ++;
            
            char tmpIp[32] = {};
            cli->GetPeerAddr().GetIP(tmpIp,
                                     (socklen_t)(sizeof tmpIp));
            oss << tmpIp;
            
            auto state = cli->GetSlaveInfo() ? cli->GetSlaveInfo()->state : 0;
            oss << ","
                << cli->GetPeerAddr().GetPort()
                << ","
                << slaveState[state]
                << "\n";
        }
    }
    
    std::string slaveInfo(oss.str());

    char buf[2048] = {};
    bool isMaster = GetMasterAddr().Empty();
    int n = snprintf(buf, sizeof buf - 1,
                 "# Replication\n"
                 "role:%s\n"
                 "connected_slaves:%d\n%s"
                 , isMaster ? "master" : "slave"
                 , index
                 , slaveInfo.c_str());

    std::ostringstream masterInfo;
    if (!isMaster)
    {
        char tmpIp[32] = {};
        masterInfo_.addr.GetIP(tmpIp, (socklen_t)(sizeof tmpIp));

        masterInfo << "master_host:"
                   << tmpIp 
                   << "\nmaster_port:" 
                   << masterInfo_.addr.GetPort()
                   << "\nmaster_link_status:";

        auto master = master_.lock();
        masterInfo << (master ? "up\n" : "down\n");
        if (!master)
            masterInfo << "master_link_down_since_seconds:"
                       << (::time(nullptr) - masterInfo_.downSince) << "\n";
    }
    
    if (!res.IsEmpty())
        res.PushData("\n", 1);

    res.PushData(buf, n);

    {
        std::string info(masterInfo.str());
        res.PushData(info.c_str(), info.size());
    }
}

}
