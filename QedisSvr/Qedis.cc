//
//  Qedis.cc
//
//  Copyright (c) 2014-2017年 Bert Young. All rights reserved.
//

#include <iostream>
#include <unistd.h>
#include <sys/wait.h>

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
#include "QModule.h"

#include "QedisLogo.h"
#include "Qedis.h"

#if QEDIS_CLUSTER
#include "QClusterClient.h"

#endif

const unsigned Qedis::kRunidSize = 40;

Qedis::Qedis() : port_(0), masterPort_(0)
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
        if (cfgFile_.empty() && ::access(av[i], R_OK) == 0)
        {
            cfgFile_ = av[i];
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
            port_ = static_cast<unsigned short>(std::atoi(av[i]));
        }
        else if (strncasecmp(av[i], "--loglevel", 10) == 0)
        {
            if (++i == ac)
            {
                return false;
            }
            logLevel_ = std::string(av[i]);
        }
        else if (strncasecmp(av[i], "--slaveof", 9) == 0)
        {
            if (i + 2 >= ac)
            {
                return false;
            }
            
            master_ = std::string(av[++i]);
            masterPort_ = static_cast<unsigned short>(std::atoi(av[++i]));
        }
        else
        {
            std::cerr << "Unknow option " << av[i] << std::endl;
            return false;
        }
    }
    
    return true;
}


std::shared_ptr<StreamSocket> Qedis::_OnNewConnection(int connfd)
{
    using namespace qedis;
    
    SocketAddr peer;
    Socket::GetPeerAddr(connfd, peer);

    auto it = std::find(g_config.centers.begin(), g_config.centers.end(), peer);
    if (it == g_config.centers.end())
    {
        auto cli(std::make_shared<QClient>());
        if (!cli->Init(connfd, peer))
            cli.reset();

        return cli;
    }
#if QEDIS_CLUSTER
    else
    {
        DBG << "Connect success to cluster " << peer.ToString();
        auto zkconn = std::make_shared<QClusterClient>();
        if (!zkconn->Init(connfd, peer))
            zkconn.reset();

        return zkconn;
    }
#endif
    
    return nullptr;
}

Time  g_now;

static void QdbCron()
{
    using namespace qedis;
    
    if (g_qdbPid != -1)
        return;
    
    if (g_now.MilliSeconds() > (g_lastQDBSave + unsigned(g_config.saveseconds)) * 1000UL &&
        QStore::dirty_ >= g_config.savechanges)
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

static void LoadDbFromDisk()
{
    using namespace qedis;
    
    //  USE AOF RECOVERY FIRST, IF FAIL, THEN RDB
    QAOFLoader aofLoader;
    if (aofLoader.Load(g_config.appendfilename.c_str()))
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

#if QEDIS_CLUSTER
static void OnConnectClusterFail(const std::vector<SocketAddr>& addrs, size_t& i)
{
    WRN << "Connect cluster failed " << addrs[i].ToString();
    if (++i >= addrs.size())
        i = 0;
    
    Timer* timer = TimerManager::Instance().CreateTimer();
    timer->Init(3 * 1000, 1);
    timer->SetCallback([=, &i]() {
        USR << "OnTimer connect to " << addrs[i].GetIP() << ":" << addrs[i].GetPort();
        Server::Instance()->TCPConnect(addrs[i], std::bind(OnConnectClusterFail, addrs, std::ref(i)));
    });
    TimerManager::Instance().AsyncAddTimer(timer);
};
#endif

bool Qedis::_Init()
{
    using namespace qedis;
    
    char runid[kRunidSize + 1] = "";
    getRandomHexChars(runid, kRunidSize);
    g_config.runid.assign(runid, kRunidSize);
    
    if (port_ != 0)
        g_config.port = port_;

    if (!logLevel_.empty())
        g_config.loglevel = logLevel_;
    
    if (!master_.empty())
    {
        g_config.masterIp = master_;
        g_config.masterPort = masterPort_;
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
    QSTORE.InitExpireTimer();
    QSTORE.InitBlockedTimer();
    QSTORE.InitEvictionTimer();
    QSTORE.InitDumpBackends();
    QPubsub::Instance().InitPubsubTimer();
    
    // Only if there is no backend, load aof or rdb
    if (g_config.backend == qedis::BackEndNone)
        LoadDbFromDisk();

    QAOFThreadController::Instance().Start();

    QSlowLog::Instance().SetThreshold(g_config.slowlogtime);
    QSlowLog::Instance().SetLogLimit(static_cast<std::size_t>(g_config.slowlogmaxlen));
    
    {
        auto cronTimer = TimerManager::Instance().CreateTimer();
        cronTimer->Init(1000 / qedis::g_config.hz);
        cronTimer->SetCallback([]() {
                QdbCron();
        });
        TimerManager::Instance().AddTimer(cronTimer);
    }

    {
        auto repTimer = TimerManager::Instance().CreateTimer();
        repTimer->Init(10 * 100);
        repTimer->SetCallback([&]() {
            QREPL.Cron();
        });
        TimerManager::Instance().AddTimer(repTimer);
    }
    
    // master ip
    if (!g_config.masterIp.empty())
    {
        QREPL.SetMasterAddr(g_config.masterIp.c_str(),
                            g_config.masterPort);
    }
    
    // load so modules
    const auto& modules = g_config.modules;
    for (const auto& mod: modules)
    {
        try
        {
            MODULES.Load(mod.c_str());
            std::cerr << "Load " << mod << " successful\n";
        }
        catch (const ModuleNoLoad& e)
        {
            std::cerr << "Load " << mod << " failed\n";
        }
        catch (const ModuleExist& e)
        {
            std::cerr << "Load " << mod << " failed because exist\n";
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Load " << mod << " failed because runtime error\n";
        }
        catch (...)
        {
            std::cerr << "Load " << mod << " failed, unknown exception\n";
        }
    }

    // output logo to console
    char logo[512] = "";
    snprintf(logo, sizeof logo - 1, qedisLogo, QEDIS_VERSION, static_cast<int>(sizeof(void*)) * 8, static_cast<int>(g_config.port)); 
    std::cerr << logo;

#if QEDIS_CLUSTER
    // cluster
    if (g_config.enableCluster)
    {
        std::vector<SocketAddr> addrs;
        for (const auto& s : g_config.centers)
        {
            addrs.push_back(SocketAddr(s));
        }

        std::function<void ()> retry = std::bind(OnConnectClusterFail, addrs, std::ref(clusterIndex_));
        Server::Instance()->TCPConnect(addrs[clusterIndex_], retry);
    }
#endif

    return  true;
}

static void CheckChild()
{
    using namespace qedis;

    if (g_qdbPid == -1 && g_rewritePid == -1)
        return;

    int statloc = 0;
    pid_t pid = wait3(&statloc,WNOHANG,NULL);

    if (pid != 0 && pid != -1)
    {
        int exit = WEXITSTATUS(statloc);
        int signal = 0;

        if (WIFSIGNALED(statloc)) signal = WTERMSIG(statloc);
        
        if (pid == g_qdbPid)
        {
            QDBSaver::SaveDoneHandler(exit, signal);
            if (QREPL.IsBgsaving())
                QREPL.OnRdbSaveDone();
            else
                QREPL.TryBgsave();
        }
        else if (pid == g_rewritePid)
        {
            INF << pid << " pid rewrite process success done.";
            QAOFThreadController::RewriteDoneHandler(exit, signal);
        }
        else
        {
            ERR << pid << " is not rdb or aof process ";
            assert (!!!"Is there any back process except rdb and aof?");
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


void Qedis::_Recycle()
{
    std::cerr << "Qedis::_Recycle: server is exiting.. BYE BYE\n";
    qedis::QAOFThreadController::Instance().Stop();
}


int main(int ac, char* av[])
{
    Qedis  svr;
    if (!svr.ParseArgs(ac - 1, av + 1))
    {
        Usage();
        return -1;
    }

    if (!svr.GetConfigName().empty())
    {
        if (!LoadQedisConfig(svr.GetConfigName().c_str(), qedis::g_config))
        {
            std::cerr << "Load config file [" << svr.GetConfigName() << "] failed!\n";
            return false;
        }
    }
    
    svr.MainLoop(qedis::g_config.daemonize);
    
    return 0;
}


