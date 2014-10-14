//
//  main.cpp
//  qedis
//
//  Created by Bert Young on 14-1-25.
//  Copyright (c) 2014年 Bert Young. All rights reserved.
//

#include <iostream>
#include "Server.h"
#include "Log/Logger.h"
#include "Timer.h"
#include "QClient.h"
#include "QStore.h"
#include "QPubsub.h"

Logger*  g_logger;

class Qedis : public Server
{
public:
    Qedis();
    ~Qedis();
    
private:
    SharedPtr<StreamSocket>   _OnNewConnection(int fd);
    bool    _Init();
    bool    _RunLogic();
    void    _Recycle();
};


Qedis::Qedis()
{
}

Qedis::~Qedis()
{
}


SharedPtr<StreamSocket>   Qedis::_OnNewConnection(int connfd)
{
    SocketAddr  peer;
    Socket::GetPeerAddr(connfd,  peer);

    LOG_INF(g_logger) << "New tcp task " << connfd << ", ip " << peer.GetIP() << " , now size = "<< TCPSize();
    
    SharedPtr<QClient>    pNewTask(new QClient());
    if (!pNewTask->Init(connfd))
        pNewTask.Reset();
    
    return  pNewTask;
}

bool Qedis::_Init()
{
    SocketAddr addr("0.0.0.0", 6379);
    
    if (!Server::TCPBind(addr))
    {
        LOG_ERR(g_logger) << "can not bind socket on port " << addr.GetPort();
        return false;
    }

    QSTORE.InitExpireTimer();
    QPubsub::Instance().InitPubsubTimer();
    
    return true;
}

bool Qedis::_RunLogic()
{
    g_now.Now();
    TimerManager::Instance().UpdateTimers(g_now);
    
    if (!Server::_RunLogic()) 
        Thread::YieldCPU();
    
    return  true;
}


void    Qedis::_Recycle()
{
#if defined(__APPLE__)
   // LOG_USR(g_logger) << __FUNCTION__ << g_mallocBytes << ", " << g_freeBytes;
#endif
    QStat::Output(PARSE_STATE);
    QStat::Output(PROCESS_STATE);
    QStat::Output(SEND_STATE);
}

int main()
{
   // g_logger = LogManager::Instance().CreateLog(0, 0, "./qedis_log");
    g_logger = LogManager::Instance().CreateLog(Logger::logALL, Logger::logALL, "./qedis_log");
    
    //daemon(1, 0);

    Qedis  svr;
    svr.MainLoop();
    
    return 0;
}


