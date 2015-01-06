#ifndef BERT_ASYNCOUTPUTFILE_H
#define BERT_ASYNCOUTPUTFILE_H

#include <functional>

#include "../Threads/Thread.h"
#include "../Buffer.h"
#include "../UnboundedBuffer.h"

class AsyncOutputFile
{
public:
    AsyncOutputFile(std::size_t defaultBufSize = 1 * 1024 * 1024);
   ~AsyncOutputFile();

    bool        OpenForWrite(const std::string& file, bool bAppend);
    bool        OpenForWrite(const char* file, bool bAppend);
    void        Close();

    void        AsyncWrite(const void* data, std::size_t len);
    void        AsyncWrite(const BufferSequence& data);
    void        Write(const void* data, std::size_t len);

    bool        IsOpen() const { return m_file != kInvalidFile; }
    std::size_t Offset() const { return m_offset; }

    bool        ExtendFileSize(std::size_t  size);
    bool        Flush();

    void        SetFlushHook(const std::function<std::size_t (const char*, std::size_t)>& hook)
    {
        m_flushHook = hook;
    }

    static bool MakeDir(const char* pDir);

    static const int   kInvalidFile = -1;
    static char* const kInvalidAddr;

private:
    bool            _AssureSpace(std::size_t  size);

    int				m_file;
    char*           m_pMemory;
    std::size_t     m_offset;
    std::size_t     m_size;

    // for async write
    Buffer          m_buffer;
    
    Mutex           m_backBufLock;
    std::size_t     m_backBytes;
    UnboundedBuffer m_backBuf;

    // flush hook
    std::function<std::size_t (const char*, std::size_t)>  m_flushHook;
};

#endif

