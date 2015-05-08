#ifndef BERT_QAOF_H
#define BERT_QAOF_H

#include <memory>
#include "MemoryFile.h"
#include "OutputBuffer.h"
#include "QString.h"
#include "Thread.h"

extern const char* const g_aofFileName;
extern const char* const g_aofTmp;

class  QAOFThreadController
{
public:
    static QAOFThreadController&  Instance();
    
    void  Start();
    void  SaveCommand(const std::vector<QString>& params, int db);
    void  Stop();
    
    void  Join();
    
    bool  ProcessTmpBuffer(BufferSequence& bf);
    void  SkipTmpBuffer(size_t  n);

    static void  AofRewriteDoneHandler(int exitcode, int bysignal);
    
private:
    QAOFThreadController() : m_lastDb(-1) {}

    class AOFThread : public Runnable
    {
        friend class QAOFThreadController;
    public:
        AOFThread() : m_alive(false) { }
        ~AOFThread();
        
        void  SetAlive()      {  m_alive = true; }
        bool  IsAlive() const {  return m_alive; }
        void  Stop()          {  m_alive = false; }
        
        bool  Open(const char* file) { return m_file.Open(file); }
        void  Close()  {    m_file.Close(); }
        void  SaveCommand(const std::vector<QString>& params);
        
        bool  Flush();
    
        virtual void Run();
        
        std::atomic<bool>   m_alive;

        MemoryFile          m_file;
        OutputBuffer        m_buf;
        
        Semaphore           m_sem;
    };
    
    void _WriteSelectDB(int db, OutputBuffer& dst);
    
    std::shared_ptr<AOFThread>  m_aofThread;
    OutputBuffer                m_aofBuffer;
    int                         m_lastDb;
    
public:
    static  pid_t               sm_aofPid;
};


class  QAOFLoader
{
public:
    QAOFLoader();
    
    bool  Load(const char* name);
    bool  IsReady() const  {  return m_state == State::AllReady; }

    const std::vector<std::vector<QString> >& GetCmds() const
    {
        return m_cmds;
    }

private:
    void _Reset();

    enum State : int8_t
    {
        Init,
        Multi,
        Param,
        Ready,
        AllReady,
    } ;

    int                   m_multi;
    std::vector<std::vector<QString> >  m_cmds;
    int m_state;
};

#endif
