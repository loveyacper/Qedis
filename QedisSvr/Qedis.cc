//
//  main.cpp
//  qedis
//
//  Created by Bert Young on 14-1-25.
//  Copyright (c) 2014年 Bert Young. All rights reserved.
//

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

#include "Server.h"
#include "Log/Logger.h"
#include "Timer.h"

#include "QClient.h"
#include "QStore.h"
#include "QCommand.h"

#include "QPubsub.h"
#include "QDB.h"
#include "QAOF.h"
#include "QConfig.h"
#include "QSlowLog.h"


class Qedis : public Server
{
public:
    Qedis();
    ~Qedis();
    
    bool  ParseArgs(int ac, char* av[]);
    
private:
    std::shared_ptr<StreamSocket>   _OnNewConnection(int fd) override;
    bool    _Init() override;
    bool    _RunLogic() override;
    void    _Recycle() override;
    
    std::string m_cfgFile;
    unsigned short m_port;
    std::string m_logLevel;
    
    std::string m_master;
    unsigned short m_masterPort;
};


Qedis::Qedis() : m_port(0), m_masterPort(0)
{
}

Qedis::~Qedis()
{
}

static void Usage()
{
    std::cerr << "Usage:  ./qedis-server [/path/to/redis.conf] [options]\n\
        ./qedis-server -v or --version\n\
        ./qedis-server -h or --help\n\
Examples:\n\
        ./qedis-server (run the server with default conf)\n\
        ./qedis-server /etc/redis/6379.conf\n\
        ./qedis-server --port 7777\n\
        ./qedis-server --port 7777 --slaveof 127.0.0.1 8888\n\
        ./qedis-server /etc/myredis.conf --loglevel verbose\n";
}

bool  Qedis::ParseArgs(int ac, char* av[])
{
    for (int i = 0; i < ac; i ++)
    {
        if (m_cfgFile.empty() && ::access(av[i], R_OK) == 0)
        {
            m_cfgFile = av[i];
            continue;
        }
        else if (strncasecmp(av[i], "-v", 2) == 0 ||
                 strncasecmp(av[i], "--version", 9) == 0)
        {
            std::cerr << "Qedis Server v="
                      << QEDIS_VERSION
                      << " bits="
                      << (sizeof(void*) == 8 ? 64 : 32)
                      << std::endl;

            exit(0);
            return true;
        }
        else if (strncasecmp(av[i], "-h", 2) == 0 ||
                 strncasecmp(av[i], "--help", 6) == 0)
        {
            Usage();
            exit(0);
            return true;
        }
        else if (strncasecmp(av[i], "--port", 6) == 0)
        {
            if (++i == ac)
            {
                return false;
            }
            m_port = static_cast<unsigned short>(std::atoi(av[i]));
        }
        else if (strncasecmp(av[i], "--loglevel", 10) == 0)
        {
            if (++i == ac)
            {
                return false;
            }
            m_logLevel = std::string(av[i]);
        }
        else if (strncasecmp(av[i], "--slaveof", 9) == 0)
        {
            if (i + 2 >= ac)
            {
                return false;
            }
            
            m_master = std::string(av[++i]);
            m_masterPort = static_cast<unsigned short>(std::atoi(av[++i]));
        }
        else
        {
            std::cerr << "Unknow option " << av[i] << std::endl;
            return false;
        }
    }
    
    return true;
}


std::shared_ptr<StreamSocket>   Qedis::_OnNewConnection(int connfd)
{
    SocketAddr  peer;
    Socket::GetPeerAddr(connfd,  peer);

    std::shared_ptr<QClient>    pNewTask(new QClient());
    if (pNewTask->Init(connfd))
    {
        const bool peerIsMaster = (peer == QReplication::Instance().GetMasterInfo().addr);
        if (peerIsMaster)
        {
            QReplication::Instance().GetMasterInfo().state = QReplState_connected;
            QReplication::Instance().SetMaster(pNewTask);
            
            pNewTask->SetName("MasterConnection");
            pNewTask->SetFlag(ClientFlag_master);
        }
    }
    else
    {
        pNewTask.reset();
    }
    
    return  pNewTask;
}

Time  g_now;

static void  QdbCron()
{
    if (g_qdbPid != -1)
        return;
    
    if (g_now.MilliSeconds() > 1000UL * (g_lastQDBSave + static_cast<unsigned>(g_config.saveseconds)) &&
        QStore::m_dirty >= g_config.savechanges)
    {
        int ret = fork();
        if (ret == 0)
        {
            {
                QDBSaver  qdb;
                qdb.Save(g_config.rdbfullname.c_str());
                std::cerr << "ServerCron child save rdb done, exiting child\n";
            }  //  make qdb to be destructed before exit
            _exit(0);
        }
        else if (ret == -1)
        {
            ERR << "fork qdb save process failed";
        }
        else
        {
            g_qdbPid = ret;
        }
            
        INF << "ServerCron save rdb file " << g_config.rdbfullname;
    }
}

class  ServerCronTimer : public Timer
{
public:
    ServerCronTimer() : Timer(1000 / g_config.hz)
    {
    }

private:
    bool _OnTimer() override
    {
        QdbCron();
        return  true;
    }
};

class  ReplicationCronTimer : public Timer
{
public:
    ReplicationCronTimer() : Timer(100 * 5)
    {
    }
    
private:
    bool _OnTimer() override
    {
        QReplication::Instance().Cron();
        return  true;
    }
};

static void LoadDbFromDisk()
{
    //  USE AOF RECOVERY FIRST, IF FAIL, THEN RDB
    QAOFLoader aofLoader;
    if (aofLoader.Load(QAOFThreadController::Instance().GetAofFile().c_str()))
    {
        const auto& cmds = aofLoader.GetCmds();
        for (const auto& cmd : cmds)
        {
            const QCommandInfo* info = QCommandTable::GetCommandInfo(cmd[0]);
            QCommandTable::ExecuteCmd(cmd, info);
        }
    }
    else
    {
        QDBLoader  loader;
        loader.Load(g_config.rdbfullname.c_str());
    }
}

bool Qedis::_Init()
{
    if (!m_cfgFile.empty())
    {
        if (!LoadQedisConfig(m_cfgFile.c_str(), g_config))
        {
            std::cerr << "Load config file [" << m_cfgFile << "] failed!\n";
            return false;
        }
    }
    else
    {
        std::cerr << "No config file specified, using the default config.\n";
    }
    
    if (m_port != 0)
        g_config.port = m_port;

    if (!m_logLevel.empty())
        g_config.loglevel = m_logLevel;
    
    if (!m_master.empty())
    {
        g_config.masterIp = m_master;
        g_config.masterPort = m_masterPort;
    }
    
    // daemon must be first, before descriptor open, threads create
    if (g_config.daemonize)
    {
        daemon(1, 0);
    }
    
    // process log
    {
        unsigned int level = ConvertLogLevel(g_config.loglevel), dest = 0;

        if (g_config.logdir == "stdout")
            dest = logConsole;
        else
            dest = logFILE;
        
        g_log = LogManager::Instance().CreateLog(level, dest, g_config.logdir.c_str());
    }
    
    SocketAddr addr(g_config.ip.c_str() , g_config.port);
    
    if (!Server::TCPBind(addr))
    {
        ERR << "can not bind socket on port " << addr.GetPort();
        return false;
    }

    QCommandTable::Init();
    QCommandTable::AliasCommand(g_config.aliases);
    QSTORE.Init(g_config.databases);
    QSTORE.m_password = g_config.password;
    QSTORE.InitExpireTimer();
    QSTORE.InitBlockedTimer();
    QPubsub::Instance().InitPubsubTimer();
    
    if (g_config.appendonly)
        QAOFThreadController::Instance().SetAofFile(g_config.appendfilename);
    
    LoadDbFromDisk();

    QAOFThreadController::Instance().Start();

    QSlowLog::Instance().SetThreshold(g_config.slowlogtime);
    
    TimerManager::Instance().AddTimer(PTIMER(new ServerCronTimer));
    TimerManager::Instance().AddTimer(PTIMER(new ReplicationCronTimer));
    
    // master ip
    if (!g_config.masterIp.empty())
    {
        QReplication::Instance().GetMasterInfo().addr.Init(g_config.masterIp.c_str(),
                                                           g_config.masterPort);
    }
    
    return  true;
}

static void CheckChild()
{
    if (g_qdbPid == -1 && g_rewritePid == -1)
        return;

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
            if (QReplication::Instance().IsBgsaving())
                QReplication::Instance().OnRdbSaveDone();
            else
                QReplication::Instance().TryBgsave();
        }
        else if (pid == g_rewritePid)
        {
            INF << pid << " rewrite process success done.";
            QAOFThreadController::RewriteDoneHandler(exitcode, bysignal);
        }
        else
        {
            ERR << pid << " is not rdb or aof process ";
            assert (false);
        }
    }
}

bool Qedis::_RunLogic()
{
    g_now.Now();
    TimerManager::Instance().UpdateTimers(g_now);
    
    CheckChild();
    
    if (!Server::_RunLogic())
        std::this_thread::sleep_for(std::chrono::microseconds(100));

    return  true;
}


void    Qedis::_Recycle()
{
    QAOFThreadController::Instance().Stop();
}


int main(int ac, char* av[])
{
    Qedis  svr;
    if (!svr.ParseArgs(ac - 1, av + 1))
    {
        Usage();
        return -1;
    }
    
    svr.MainLoop();
    
    return 0;
}


