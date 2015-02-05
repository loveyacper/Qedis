
#ifndef BERT_SERVER_H
#define BERT_SERVER_H

#include "TaskManager.h"

struct SocketAddr;

class Server    // Should singleton
{
protected:
    virtual bool _RunLogic();
    virtual void _Recycle() { }
    virtual bool _Init() = 0;
    
    Server();
    
public:
    virtual ~Server();

    bool  TCPBind(const SocketAddr&  listenAddr);
    void  TCPReconnect(const SocketAddr& peer);

    static Server*  Instance() {   return   sm_instance;  }

    bool IsTerminate() const { return m_bTerminate; }
    void Terminate()  { m_bTerminate = true; }

    void MainLoop();
    void NewConnection(int sock, bool retry = false);

    size_t  TCPSize() const  {  return  m_tasks.TCPSize(); }

    // SIGHUP handler, in fact, you should load config use this function;
    virtual void ReloadConfig()    { }

    static void IntHandler(int sig);
    static void HupHandler(int sig);

    std::shared_ptr<StreamSocket>  FindTCP(unsigned int id) const { return m_tasks.FindTCP(id); }

private:
    virtual std::shared_ptr<StreamSocket>   _OnNewConnection(int tcpsock);

    volatile bool m_bTerminate;
    Internal::TaskManager   m_tasks;
    bool          m_reloadCfg;
    static Server*   sm_instance;
};

#endif

