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
    
    SocketAddr  peer_;

private:
    bool _OnTimer()
    {
        USR << " : OnTimer reconnect to " << peer_.GetIP() << ":" << peer_.GetPort();
        Server::Instance()->TCPConnect(peer_, true);

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
        Server::Instance()->reloadCfg_ = true;
}

Server* Server::sinstance_ = nullptr;

std::set<int>   Server::slistenSocks_;

Server::Server() : bTerminate_(false), reloadCfg_(false)
{
    if (sinstance_ == NULL)
        sinstance_ = this;
    else
        ::abort();
}

Server::~Server()
{
    sinstance_ = NULL;
}

bool Server::_RunLogic() 
{
    return tasks_.DoMsgParse();
}

bool Server::TCPBind(const SocketAddr& addr)
{
    using Internal::ListenSocket;

    std::shared_ptr<ListenSocket> pServerSocket(new ListenSocket);

    if (pServerSocket->Bind(addr))
    {
        slistenSocks_.insert(pServerSocket->GetSocket());
        return true;
    }
    
    return  false;
}


void Server::TCPReconnect(const SocketAddr& peer)
{
    INF << __FUNCTION__ << peer.GetIP();
    std::shared_ptr<ReconnTimer>  pTimer(new ReconnTimer(2 * 1000));  // TODO : flexible
    pTimer->peer_ = peer;

    TimerManager::Instance().AsyncAddTimer(pTimer);
}

void Server::TCPConnect(const SocketAddr& peer, bool retry)
{
    INF << __FUNCTION__ << peer.GetIP();
    
    std::shared_ptr<ClientSocket>  pClient(new ClientSocket(retry));
    pClient->Connect(peer);
}

void Server::MainLoop(bool daemon)
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

    ::pthread_atfork(nullptr, nullptr, AtForkHandler);
    
    ::srand(static_cast<unsigned int>(time(NULL)));
    ::srandom(static_cast<unsigned int>(time(NULL)));

    // daemon must be first, before descriptor open, threads create
    if (daemon)
    {
        ::daemon(1, 0);
    }
    
    if (NetThreadPool::Instance().StartAllThreads() &&
        _Init() &&
        LogManager::Instance().StartLog())
    {
        while (!bTerminate_)
        {
            if (reloadCfg_)
            {
                ReloadConfig();
                reloadCfg_ = false;
            }

            _RunLogic();
        }
    }

    tasks_.Clear();
    _Recycle();
    NetThreadPool::Instance().StopAllThreads();
    LogManager::Instance().StopLog();
    
    ThreadPool::Instance().JoinAll();
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

    pNewTask->OnConnect();
    pNewTask->SetReconn(needReconn);

    if (NetThreadPool::Instance().AddSocket(pNewTask, EventTypeRead | EventTypeWrite))
        tasks_.AddTask(pNewTask);
}

void   Server::AtForkHandler()
{
    for (auto sock : slistenSocks_)
    {
        close(sock);
    }
}

void   Server::DelListenSock(int sock)
{
    if (sock == INVALID_SOCKET)
        return;
    
    auto n = slistenSocks_.erase(sock);

    if (n != 1)
        ERR << "DelListenSock failed  " << sock;
    else
        INF << "DelListenSock succ " << sock;
}
