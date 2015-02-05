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
#include "QCommand.h"

class Qedis : public Server
{
public:
    Qedis();
    ~Qedis();
    
private:
    std::shared_ptr<StreamSocket>   _OnNewConnection(int fd);
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


std::shared_ptr<StreamSocket>   Qedis::_OnNewConnection(int connfd)
{
    SocketAddr  peer;
    Socket::GetPeerAddr(connfd,  peer);

    std::shared_ptr<QClient>    pNewTask(new QClient());
    if (!pNewTask->Init(connfd))
        pNewTask.reset();
    
    return  pNewTask;
}

bool Qedis::_Init()
{
    SocketAddr addr("0.0.0.0", 6379);
    
    if (!Server::TCPBind(addr))
    {
        ERR << "can not bind socket on port " << addr.GetPort();
        return false;
    }

    QCommandTable::Init();
    QSTORE.InitExpireTimer();
    QPubsub::Instance().InitPubsubTimer();
    
    return true;
}
Time  g_now;
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
    QStat::Output(PARSE_STATE);
    QStat::Output(PROCESS_STATE);
    QStat::Output(SEND_STATE);
}

int main()
{
    g_log = LogManager::Instance().NullLog();
    g_log = LogManager::Instance().CreateLog(logALL, logALL, "./qedis_log");
    
    //daemon(1, 0);

    Qedis  svr;
    svr.MainLoop();
    
    return 0;
}


