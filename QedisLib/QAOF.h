#ifndef BERT_QAOF_H
#define BERT_QAOF_H

#include <memory>
#include <future>
#include "Log/MemoryFile.h"
#include "AsyncBuffer.h"
#include "QString.h"
#include "QStore.h"

namespace qedis
{

extern pid_t g_rewritePid;

class  QAOFThreadController
{
public:
    static QAOFThreadController&  Instance();
    
    QAOFThreadController(const QAOFThreadController& ) = delete;
    void  operator= (const QAOFThreadController& ) = delete;

    void  Start();
    void  Stop();
    void  Join();
    
    void  SaveCommand(const std::vector<QString>& params, int db);
    bool  ProcessTmpBuffer(BufferSequence& bf);
    void  SkipTmpBuffer(size_t  n);
    
    static void  RewriteDoneHandler(int exitcode, int bysignal);
    
private:
    QAOFThreadController() : lastDb_(-1) {}

    class AOFThread
    {
        friend class QAOFThreadController;
    public:
        AOFThread() : alive_(false) { }
        ~AOFThread();
        
        void  SetAlive()      {  alive_ = true; }
        bool  IsAlive() const {  return alive_; }
        void  Stop()          {  alive_ = false; }
        
        //void  Close();
        void  SaveCommand(const std::vector<QString>& params);
        
        bool  Flush();
    
        void  Run();
        
        std::atomic<bool>   alive_;

        OutputMemoryFile    file_;
        AsyncBuffer         buf_;
        
        std::promise<void>  pro_; // Effective modern C++ : Item 39
    };
    
    void _WriteSelectDB(int db, AsyncBuffer& dst);
    
    std::shared_ptr<AOFThread>  aofThread_;
    AsyncBuffer                 aofBuffer_;
    int                         lastDb_;
};


class  QAOFLoader
{
public:
    QAOFLoader();
    
    bool  Load(const char* name);

    const std::vector<std::vector<QString> >& GetCmds() const
    {
        return cmds_;
    }

private:
    std::vector<std::vector<QString> > cmds_;
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
    
    for (const auto& s : params)
    {
        WriteBulkString(s, dst);
    }
}

}

#endif
