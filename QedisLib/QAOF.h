#ifndef BERT_QAOF_H
#define BERT_QAOF_H

#include <memory>
#include <future>
#include "Log/MemoryFile.h"
#include "OutputBuffer.h"
#include "QString.h"

#include "QStore.h"

extern const char* const g_aofFileName;
extern const char* const g_aofTmp;

extern pid_t             g_rewritePid;

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

    static void  RewriteDoneHandler(int exitcode, int bysignal);
    
private:
    QAOFThreadController() : m_lastDb(-1) {}

    class AOFThread
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

        OutputMemoryFile    m_file;
        OutputBuffer        m_buf;
        
        std::promise<void>  m_pro; // Effective modern C++ : Item 39
    };
    
    void _WriteSelectDB(int db, OutputBuffer& dst);
    
    std::shared_ptr<AOFThread>  m_aofThread;
    OutputBuffer                m_aofBuffer;
    int                         m_lastDb;
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

template <typename DEST>
inline void  WriteBulkString(const char* str, size_t strLen, DEST& dst)
{
    char    tmp[32];
    size_t  n = snprintf(tmp, sizeof tmp, "$%lu\r\n", strLen);
    
    dst.Write(tmp, n);
    dst.Write(str, strLen);
    dst.Write("\r\n", 2);
}


template <typename DEST>
inline void  WriteBulkString(const QString& str, DEST& dst)
{
    WriteBulkString(str.data(), str.size(), dst);
}

template <typename DEST>
inline void  WriteMultiBulkLong(long val, DEST& dst)
{
    char    tmp[32];
    size_t  n = snprintf(tmp, sizeof tmp, "*%lu\r\n", val);
    dst.Write(tmp, n);
}

template <typename DEST>
inline void  WriteBulkLong(long val, DEST& dst)
{
    char    tmp[32];
    size_t  n = snprintf(tmp, sizeof tmp, "%lu", val);
    
    WriteBulkString(tmp, n, dst);
}


template <typename DEST>
inline void SaveCommand(const std::vector<QString>& params, DEST& dst)
{
    WriteMultiBulkLong(params.size(), dst);
    
    for (size_t i = 0; i < params.size(); ++ i)
    {
        WriteBulkString(params[i], dst);
    }
}


#endif
