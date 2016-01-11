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
    std::lock_guard<std::mutex>  guard(lock_);
    newTasks_.push_back(task);
    ++ newCnt_;

    return  true;    
}

TaskManager::PTCPSOCKET  TaskManager::FindTCP(unsigned int id) const
{
    if (id > 0)
    {
        std::map<int, PTCPSOCKET>::const_iterator it = tcpSockets_.find(id);
        if (it != tcpSockets_.end())
            return  it->second;
    }
           
    return  PTCPSOCKET();
}

bool TaskManager::_AddTask(PTCPSOCKET task)
{   
    bool succ = tcpSockets_.insert(std::map<int, PTCPSOCKET>::value_type(task->GetID(), task)).second;
    return  succ;    
}


void TaskManager::_RemoveTask(std::map<int, PTCPSOCKET>::iterator& it)    
{
    tcpSockets_.erase(it ++);
}


bool TaskManager::DoMsgParse()
{
    if (newCnt_ > 0 && lock_.try_lock())
    {
        NEWTASKS_T  tmpNewTask;
        tmpNewTask.swap(newTasks_);
        newCnt_ = 0;
        lock_.unlock();

        for (NEWTASKS_T::iterator it(tmpNewTask.begin());
            it != tmpNewTask.end();
            ++ it)
        {
            if (!_AddTask(*it))
            {
                WITH_LOG(ERR << "Why can not insert tcp socket " \
                    << (*it)->GetSocket() \
                    << ", id = " \
                    << (*it)->GetID());
            }
        }
    }

    bool  busy = false;

    for (std::map<int, PTCPSOCKET>::iterator it(tcpSockets_.begin());
         it != tcpSockets_.end();
         )
    {
        if (!it->second || it->second->Invalid())
        {
            WITH_LOG(USR << "Remove tcp task from do msg parse " << (it->second ? it->second->GetSocket() : -1));
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

