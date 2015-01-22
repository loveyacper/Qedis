#include <assert.h>
#include <iostream>

#if defined(__APPLE__)
#include <unistd.h>
#endif

#include "OutputBuffer.h"

using std::size_t;

OutputBuffer::OutputBuffer(size_t size) : m_buffer(size),
                                          m_backBytes(0)
{
}

OutputBuffer::~OutputBuffer()
{
    assert (m_buffer.IsEmpty());
    assert (m_backBytes == 0);
}


void   OutputBuffer::AsyncWrite(const void* data, size_t len)
{
    if (m_backBytes > 0 || !m_buffer.PushData(data, len))
    {
        std::lock_guard<std::mutex>  guard(m_backBufLock);
     
        m_backBuf.PushData(data, len);
        m_backBytes += len;
        assert (m_backBytes == m_backBuf.ReadableSize());
    }
}

void   OutputBuffer::AsyncWrite(const BufferSequence& data)
{
    size_t len = data.TotalBytes();

    if (m_backBytes > 0 || m_buffer.WritableSize() < len)
    {
        std::lock_guard<std::mutex>  guard(m_backBufLock);

        if (m_backBytes > 0 || m_buffer.WritableSize() < len)
        {
            for (size_t i = 0; i < data.count; ++ i)
            {
                m_backBuf.PushData(data.buffers[i].iov_base,
                                   data.buffers[i].iov_len);
            }
        
            m_backBytes += len;
            assert (m_backBytes == m_backBuf.ReadableSize());

            return;
        }
    }
    
    assert(m_backBytes == 0 && m_buffer.WritableSize() >= len);

    for (size_t i = 0; i < data.count; ++ i)
    {
        m_buffer.PushData(data.buffers[i].iov_base, data.buffers[i].iov_len);
    }
}

void  OutputBuffer::ProcessBuffer(BufferSequence& data)
{
    data.count = 0;
    
    if (!m_buffer.IsEmpty())
    {
        size_t nLen = m_buffer.ReadableSize();

        m_buffer.GetDatum(data, nLen);
        assert (nLen == data.TotalBytes());
    }
    else
    {
        if (m_backBytes > 0 && m_backBufLock.try_lock())
        {
            m_backBytes = 0;
            m_tmpBuf.Swap(m_backBuf);
            m_backBufLock.unlock();
            
            data.count = 1;
            data.buffers[0].iov_base = m_tmpBuf.ReadAddr();
            data.buffers[0].iov_len  = m_tmpBuf.ReadableSize();
        }
    }
}

void  OutputBuffer::Skip(size_t  size)
{
    if (!m_tmpBuf.IsEmpty())
    {
        assert(size == m_tmpBuf.ReadableSize());
        UnboundedBuffer().Swap(m_tmpBuf);
    }
    else
    {
        assert(m_buffer.ReadableSize() >= size);
        m_buffer.AdjustReadPtr(size);
    }
}

