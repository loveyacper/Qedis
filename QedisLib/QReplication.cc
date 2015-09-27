
#include <unistd.h>
#include <iostream>

#include "QClient.h"
#include "QConfig.h"
#include "QCommon.h"
#include "QDB.h"
#include "QReplication.h"

#include "QAOF.h" // FOR save changes commands


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
                std::cerr << "can not open rdb when replication\n";
                return;
                // fatal error;
            }
            
            std::size_t   size = std::size_t(-1);
            const char* data = rdb.Read(size);
            
            // $file_len + filedata
            UnboundedBuffer  tmp;
            WriteBulkLong((long)size, tmp);
            
            cli->SendPacket(tmp.ReadAddr(), tmp.ReadableSize());
            cli->SendPacket(data, size);
            cli->SendPacket(m_buffer.ReadAddr(), m_buffer.ReadableSize());
            
            std::cerr << "Send to slave rdb " << size << ", buffer " << m_buffer.ReadableSize() << std::endl;
            
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
            std::cerr << "QReplication save rdb done, exiting child\n";
        }
        exit(0);
    }
    else if (ret == -1)
    {
        // fatal error
        std::cerr << "QReplication save rdb FATAL ERROR\n";
        _OnStartBgsave(false); // 设置slave状态
        return;
    }
    else
    {
        std::cerr << "QReplication save rdb START\n";
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
                std::cerr << "_OnStartBgsave Set cli state " << cli->GetName();
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
    
#if 0
    for (const auto& wptr : m_slaves)
    {
        auto cli = wptr.lock();
        if (!cli || cli->GetSlaveInfo()->state != QSlaveState_online)
            continue;
        
        
        
    }
#endif
    
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
