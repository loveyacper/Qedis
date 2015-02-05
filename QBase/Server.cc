#include <signal.h>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <sys/resource.h>
#include "Server.h"
#include "ListenSocket.h"
#include "ClientSocket.h"
#include "StreamSocket.h"
#include "NetThreadPool.h"
#include "Timer.h"
#include "Log/Logger.h"

using Internal::NetThreadPool;

class ReconnTimer : public Timer
{   
public:
    ReconnTimer(int interval) : Timer(interval, 1)
    {
    }
    
    SocketAddr  m_peer;

private:
    bool _OnTimer()
    {
        USR << " : OnTimer reconnect to " << m_peer.GetIP();

        std::shared_ptr<ClientSocket>  pClient(new ClientSocket);
        pClient->Connect(m_peer);

        return false;
    }
};

void Server::IntHandler(int signum)
{
    if (Server::Instance() != NULL)
        Server::Instance()->Terminate();
}

void Server::HupHandler(int signum)
{
    if (Server::Instance() != NULL)
        Server::Instance()->m_reloadCfg = true;
}

Server* Server::sm_instance = NULL;

Server::Server() : m_bTerminate(false), m_reloadCfg(false)
{
    if (sm_instance == NULL)
        sm_instance = this;
    else
        ::abort();
}

Server::~Server()
{
    sm_instance = NULL;
}

bool Server::_RunLogic() 
{
    return m_tasks.DoMsgParse();
}

bool Server::TCPBind(const SocketAddr& addr)
{
    using Internal::ListenSocket;

    std::shared_ptr<ListenSocket> pServerSocket(new ListenSocket);

    return  pServerSocket->Bind(addr);
}


void Server::TCPReconnect(const SocketAddr& peer)
{
    INF << __FUNCTION__ << peer.GetIP();
    std::shared_ptr<ReconnTimer>  pTimer(new ReconnTimer(2 * 1000));  // TODO : flexible
    pTimer->m_peer = peer;

    TimerManager::Instance().AsyncAddTimer(pTimer);
}

void Server::MainLoop()
{
    struct sigaction sig;
    ::memset(&sig, 0, sizeof(sig));
    sig.sa_handler = &Server::IntHandler;
    sigaction(SIGINT, &sig, NULL);
    sigaction(SIGQUIT, &sig, NULL);
    sigaction(SIGABRT, &sig, NULL);
    sigaction(SIGTERM, &sig, NULL);
    sig.sa_handler =  &Server::HupHandler;
    sigaction(SIGHUP, &sig, NULL);

    sig.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sig, NULL);

    ::srand(static_cast<unsigned int>(time(NULL)));

    // set the max fd number
    struct rlimit   rlim; 
    getrlimit(RLIMIT_NOFILE, &rlim); 
    if (rlim.rlim_max < 30000) 
    { 
        rlim.rlim_max = rlim.rlim_cur = 30000;
        if (0 != setrlimit(RLIMIT_NOFILE, &rlim)) 
        { 
            perror("setrlimit error "); 
            ERR << "Failed to setrlimit ";
        } 
    }

//  int threadNum = (GetCpuNum() + 1) / 2;
    if (NetThreadPool::Instance().StartAllThreads() &&
        _Init() &&
        LogManager::Instance().StartLog())
    {
        while (!m_bTerminate)
        {
            if (m_reloadCfg)
            {
                ReloadConfig();
                m_reloadCfg = false;
            }

            _RunLogic();
        }
    }

    m_tasks.Clear();
    _Recycle();
    NetThreadPool::Instance().StopAllThreads();
    LogManager::Instance().StopLog();
    
    ThreadPool::Instance().StopAllThreads();
}


std::shared_ptr<StreamSocket>   Server::_OnNewConnection(int tcpsock)
{
    WRN << "implement your tcp accept, now close socket " << tcpsock;
    return std::shared_ptr<StreamSocket>((StreamSocket* )0);
}

void  Server::NewConnection(int  sock, bool needReconn)
{
    if (sock == INVALID_SOCKET)
        return;

    std::shared_ptr<StreamSocket>  pNewTask = _OnNewConnection(sock);
    
    if (!pNewTask)
    {
        Socket::CloseSocket(sock);
        return;
    }

    pNewTask->SetReconn(needReconn);

    if (NetThreadPool::Instance().AddSocket(pNewTask, EventTypeRead | EventTypeWrite))
        m_tasks.AddTask(pNewTask);
}

