#ifndef BERT_OUTPUTBUFFER_H
#define BERT_OUTPUTBUFFER_H

#include <mutex>

#include "../Buffer.h"
#include "../UnboundedBuffer.h"

class OutputBuffer
{
public:
    explicit
    OutputBuffer(std::size_t  size = 1 * 1024 * 1024);
   ~OutputBuffer();

    void        AsyncWrite(const void* data, std::size_t len);
    void        AsyncWrite(const BufferSequence& data);

    void        ProcessBuffer(BufferSequence& data);
    void        Skip(std::size_t  size);

private:
    // for async write
    Buffer          m_buffer;
    
    std::mutex      m_backBufLock;
    std::size_t     m_backBytes;
    UnboundedBuffer m_backBuf;
    UnboundedBuffer m_tmpBuf;
};

#endif

