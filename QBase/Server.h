
#ifndef BERT_SERVER_H
#define BERT_SERVER_H

#include <set>
#include <functional>
#include "TaskManager.h"

struct SocketAddr;

class Server    // Should singleton
{
protected:
    virtual bool _RunLogic();
    virtual void _Recycle() { }
    virtual bool _Init() = 0;
    
    Server();

    Server(const Server& ) = delete;
    void operator= (const Server& ) = delete;
    
public:
    virtual ~Server();

    bool  TCPBind(const SocketAddr&  listenAddr);
    void  TCPReconnect(const SocketAddr& peer);
    void  TCPConnect(const SocketAddr& peer, const std::function<void()>* cb);

    static Server*  Instance() {   return   sinstance_;  }

    bool IsTerminate() const { return bTerminate_; }
    void Terminate()  { bTerminate_ = true; }

    void MainLoop(bool daemon = false);
    void NewConnection(int sock, const std::function<void ()>& cb = std::function<void ()>());

    void TCPConnect(const SocketAddr& peer);
    void TCPConnect(const SocketAddr& peer, const std::function<void ()>& cb);

    size_t  TCPSize() const  {  return  tasks_.TCPSize(); }

    // SIGHUP handler, in fact, you should load config use this function;
    virtual void ReloadConfig()    { }

    static void IntHandler(int sig);
    static void HupHandler(int sig);

    std::shared_ptr<StreamSocket>  FindTCP(unsigned int id) const { return tasks_.FindTCP(id); }
    
    static void AtForkHandler();
    static void DelListenSock(int sock);

private:
    virtual std::shared_ptr<StreamSocket>   _OnNewConnection(int tcpsock);
    void _TCPConnect(const SocketAddr& peer, const std::function<void()>* cb = nullptr);

    std::atomic<bool> bTerminate_;
    Internal::TaskManager   tasks_;
    bool          reloadCfg_;
    static Server*   sinstance_;
    
    static std::set<int>  slistenSocks_;
};

#endif

