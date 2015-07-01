#ifndef BERT_TASKMANAGER_H
#define BERT_TASKMANAGER_H

#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>

class StreamSocket;

namespace Internal
{

class TaskManager 
{
    typedef std::shared_ptr<StreamSocket>     PTCPSOCKET;
    typedef std::vector<PTCPSOCKET>     NEWTASKS_T;

public:
    TaskManager() : m_newCnt(0) { }
    ~TaskManager();
    
    bool AddTask(PTCPSOCKET );

    bool Empty() const { return m_tcpSockets.empty(); }
    void Clear()  { m_tcpSockets.clear(); }
    PTCPSOCKET  FindTCP(unsigned int id) const;
    
    size_t      TCPSize() const  {  return  m_tcpSockets.size(); }

    bool DoMsgParse();

private:
    bool _AddTask(PTCPSOCKET task);
    void _RemoveTask(std::map<int, PTCPSOCKET>::iterator& );
    std::map<int, PTCPSOCKET>  m_tcpSockets;

    // Lock for new tasks
    std::mutex      m_lock;
    NEWTASKS_T      m_newTasks; 
    std::atomic<int> m_newCnt; // vector::empty() is not thread-safe !!!
};

}

#endif

