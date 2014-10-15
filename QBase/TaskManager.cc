#include <cassert>
#include "TaskManager.h"
#include "StreamSocket.h"
#include "Log/Logger.h"

namespace Internal
{

TaskManager::~TaskManager()
{
    assert(Empty() && "Why you do not clear container before exit?");
}
    
     
bool TaskManager::AddTask(PTCPSOCKET  task)    
{   
    ScopeMutex guard(m_lock);
    m_newTasks.push_back(task);
    ++ m_newCnt;

    return  true;    
}

TaskManager::PTCPSOCKET  TaskManager::FindTCP(unsigned int id) const
{
    if (id > 0)
    {
        std::map<int, PTCPSOCKET>::const_iterator it = m_tcpSockets.find(id);
        if (it != m_tcpSockets.end())
            return  it->second;
    }
           
    return  PTCPSOCKET();
}

bool TaskManager::_AddTask(PTCPSOCKET task)
{   
    bool succ = m_tcpSockets.insert(std::map<int, PTCPSOCKET>::value_type(task->GetID(), task)).second;
    return  succ;    
}


void TaskManager::_RemoveTask(std::map<int, PTCPSOCKET>::iterator& it)    
{
    m_tcpSockets.erase(it ++);
}


bool TaskManager::DoMsgParse()
{
    if (m_newCnt > 0 && m_lock.TryLock())
    {
        NEWTASKS_T  tmpNewTask;
        tmpNewTask.swap(m_newTasks);
        m_newCnt = 0;
        m_lock.Unlock();

        for (NEWTASKS_T::iterator it(tmpNewTask.begin());
            it != tmpNewTask.end();
            ++ it)
        {
            if (!_AddTask(*it))
            {
                ERR << "Why can not insert tcp socket "
                    << (*it)->GetSocket()
                    << ", id = "
                    << (*it)->GetID();
            }
        }
    }

    bool  busy = false;

    for (std::map<int, PTCPSOCKET>::iterator it(m_tcpSockets.begin());
         it != m_tcpSockets.end();
         )
    {
        if (!it->second || it->second->Invalid())
        {
            USR << "Remove tcp task from do msg parse " << (it->second ? it->second->GetSocket() : -1);
            _RemoveTask(it);
        }
        else
        {
            if (it->second->DoMsgParse() && !busy)
                busy = true;

            ++ it;
        }
    }

    return  busy;
}

}

