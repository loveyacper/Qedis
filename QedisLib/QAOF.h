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
    bool  Update();
    
    void  Join();
    
    bool  ProcessTmpBuffer(BufferSequence& bf);
    void  SkipTmpBuffer(size_t  n);
    /*
     MemoryFile  file;
     select db first;
     savedata(const string& key, const qobject& obj);
     set expired;
     savelist;  // rpush key elem;
     savestring;  // set key val;
     saveset; // sadd key elem;
     savesset;  // zadd key score member
     savehash;  // hset key key value
     static void  Rewrite();
     */
    static void  AofRewriteDoneHandler(int exitcode, int bysignal);
    
private:
    QAOFThreadController() : m_lastDb(-1) {}

    /* when thread starting, the file is open first.
     * when exiting, file will be closed and sync.
     */
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
    OutputBuffer                m_aofBuffer; // when rewrite, thread is stopped
    int                         m_lastDb;
    
public:
    static  pid_t               sm_aofPid;
};


#endif
