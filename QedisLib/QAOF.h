#ifndef BERT_QAOF_H
#define BERT_QAOF_H

#include <memory>
#include "MemoryFile.h"
#include "OutputBuffer.h"
#include "QString.h"
#include "Thread.h"

extern const char* const g_aofFileName;

class QAOFFile
{
public:
    ~QAOFFile();
    
    static  QAOFFile& Instance();

    bool    Open(const char* );
    void    SaveCommand(const std::vector<QString>& params);
    bool    Loop();
    bool    Sync();
    static  void  SaveDoneHandler(int exitcode, int bysignal);

private:
    QAOFFile();
    
    MemoryFile     m_file;
    OutputBuffer   m_buf;
};


class  QAOFThread
{
public:
    static QAOFThread&  Instance();
    
    void  Start();
    void  Stop();
    bool  Update();
    
private:
    QAOFThread() {}
    
    class AOFThread : public Runnable
    {
    public:
        AOFThread() : m_alive(false) {}
        
        void  SetAlive()      {  m_alive = true; }
        bool  IsAlive() const {  return m_alive; }
        void  Stop()          {  m_alive = false; }
    
        virtual void Run();
    
    private:
        std::atomic<bool>   m_alive;
    };
    
    std::shared_ptr<AOFThread>  m_aofThread;
};

extern pid_t  g_aofPid;


#endif
