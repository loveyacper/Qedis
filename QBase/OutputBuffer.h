#ifndef BERT_OUTPUTBUFFER_H
#define BERT_OUTPUTBUFFER_H

#include <mutex>
#include <atomic>

#include "Buffer.h"
#include "UnboundedBuffer.h"

class OutputBuffer
{
public:
    explicit
    OutputBuffer(std::size_t  size = 128 * 1024);
   ~OutputBuffer();

    void        Write(const void* data, std::size_t len);
    void        Write(const BufferSequence& data);

    void        ProcessBuffer(BufferSequence& data);
    void        Skip(std::size_t  size);

private:
    // for async write
    Buffer          buffer_;
    
    std::mutex      backBufLock_;
    std::atomic<std::size_t>    backBytes_;
    qedis::UnboundedBuffer backBuf_;
    qedis::UnboundedBuffer tmpBuf_;
};

#endif

