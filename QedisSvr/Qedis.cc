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
#include "QDB.h"
#include "QAOF.h"


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
    QSTORE.InitBlockedTimer();
    QPubsub::Instance().InitPubsubTimer();
    
    //  USE AOF RECOVERY FIRST, IF FAIL, THEN RDB
    QAOFLoader aofLoader;
    if (aofLoader.Load(g_aofFileName))
    {
        const auto& cmds = aofLoader.GetCmds();
        for (const auto& cmd : cmds)
        {
            const QCommandInfo* info = QCommandTable::GetCommandInfo(cmd[0]);
            UnboundedBuffer reply;
            QCommandTable::ExecuteCmd(cmd, info, reply);
        }
    }
    else
    {
        QDBLoader  loader;
        loader.Load(g_qdbFile);
    }
    
    QAOFThreadController::Instance().Start();
    
    return true;
}

Time  g_now;

bool Qedis::_RunLogic()
{
    g_now.Now();
    TimerManager::Instance().UpdateTimers(g_now);
    
    if (g_qdbPid != -1 || QAOFThreadController::sm_aofPid != -1)
    {
        int    statloc;

        pid_t  pid = wait3(&statloc,WNOHANG,NULL);
        if (pid != 0 && pid != -1)
        {
            int exitcode = WEXITSTATUS(statloc);
            int bysignal = 0;

            if (WIFSIGNALED(statloc)) bysignal = WTERMSIG(statloc);
            
            if (pid == g_qdbPid)
            {
                QDBSaver::SaveDoneHandler(exitcode, bysignal);
            }
            else if (pid == QAOFThreadController::sm_aofPid )
            {
                INF << pid << " aof process success done.";
                QAOFThreadController::AofRewriteDoneHandler(exitcode, bysignal);
            }
            else
            {
                ERR << pid << " is not rdb or aof process ";
                assert (false);
            }
        }
    }
    
    if (!Server::_RunLogic())
        Thread::YieldCPU();

    return  true;
}


void    Qedis::_Recycle()
{
    QAOFThreadController::Instance().Stop();
    
    QStat::Output(PARSE_STATE);
    QStat::Output(PROCESS_STATE);
    QStat::Output(SEND_STATE);
}

int main()
{
    //g_log = LogManager::Instance().NullLog();
    g_log = LogManager::Instance().CreateLog(logALL, logALL, "./qedis_log");
    
    //daemon(1, 0);

    Qedis  svr;
    svr.MainLoop();
    
    return 0;
}


