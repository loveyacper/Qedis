#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>

#if defined(__APPLE__)
#include <unistd.h>
#endif

#include "AsyncOutputFile.h"

using std::size_t;
    
char* const AsyncOutputFile::kInvalidAddr = reinterpret_cast<char*>(-1);

AsyncOutputFile::AsyncOutputFile(size_t size) : m_file(kInvalidFile),
                                                m_pMemory(kInvalidAddr),
                                                m_offset(0),
                                                m_size(0),
                                                m_buffer(size),
                                                m_backBytes(0)
{
}

AsyncOutputFile::~AsyncOutputFile()
{
    Close();
}

bool AsyncOutputFile::ExtendFileSize(size_t  size)
{
    assert (m_file != kInvalidFile);

    if (m_size >= size)
        return true;

    if (m_size != 0)
        ::munmap(m_pMemory, m_size);

    ::ftruncate(m_file, size);
    m_size = size;
    m_pMemory = (char* )::mmap(0, size, PROT_WRITE, MAP_SHARED, m_file, 0);

    if (m_pMemory == kInvalidAddr)
        perror("mmap failed ");
    
    return   m_pMemory != kInvalidAddr;
}

bool  AsyncOutputFile::OpenForWrite(const std::string&  file, bool bAppend)
{
    return OpenForWrite(file.c_str(), bAppend);
}

bool  AsyncOutputFile::OpenForWrite(const char* file, bool bAppend)
{
    Close();

    m_file = ::open(file, O_RDWR | O_CREAT, 0644);

    if (m_file == kInvalidFile)
    {
        char err[128];
        snprintf(err, sizeof err - 1, "OpenWriteOnly %s failed\n", file);
        perror(err);
        return false;
    }

    if (bAppend)
    {
        struct stat st;
        fstat(m_file, &st);
        m_offset  = st.st_size;
    }
    else
    {
        m_offset  = 0;
    }

    return true;
}

void  AsyncOutputFile::Close()
{
    if (m_file != kInvalidFile)
    {
        ::munmap(m_pMemory, m_size);
        ::ftruncate(m_file, m_offset);
        ::close(m_file);

        m_file      = kInvalidFile;
        m_size      = 0;
        m_pMemory   = kInvalidAddr;
        m_offset    = 0;
    }
}

// producer
void   AsyncOutputFile::AsyncWrite(const void* data, size_t len)
{
    if (m_backBytes > 0 || !m_buffer.PushData(data, len))
    {
        ScopeMutex   lock(m_backBufLock);

        std::cerr << "begin push to back buffer " << len << std::endl;
        m_backBuf.PushData(data, len);
        m_backBytes += len;
        assert (m_backBytes == m_backBuf.ReadableSize());
        std::cerr << "after push to back buffer, size " << m_backBytes << std::endl;
    }
}

// producer
void   AsyncOutputFile::AsyncWrite(const BufferSequence& data)
{
    size_t len = data.TotalBytes();

    if (m_backBytes > 0 || m_buffer.WritableSize() < len)
    {
        ScopeMutex   lock(m_backBufLock);

        for (size_t i = 0; i < data.count; ++ i)
        {
            bool bSucc = m_backBuf.PushData(data.buffers[i].iov_base, data.buffers[i].iov_len);
            assert (bSucc);
        }
        
        m_backBytes += len;
        assert (m_backBytes == m_backBuf.ReadableSize());
    }
    else
    {
        for (size_t i = 0; i < data.count; ++ i)
        {
            bool bSucc = m_buffer.PushData(data.buffers[i].iov_base, data.buffers[i].iov_len);
            assert (bSucc);
        }
    }
}

// consumer
void   AsyncOutputFile::Write(const void* data, size_t len)
{
    _AssureSpace(len);
                
    ::memcpy(m_pMemory + m_offset, data, len);
    m_offset += len;
}
    
bool   AsyncOutputFile::_AssureSpace(size_t  size)
{
    size_t  newSize = m_size;
    while (m_offset + size > newSize)
    {
        if (newSize == 0)
            newSize = 16 * 1024 * 1024;
        else
            newSize <<= 1;
    }

    return ExtendFileSize(newSize);
}

// consumer
bool   AsyncOutputFile::Flush()
{
    if (!IsOpen())  return false;

    bool  hasData = false;

    if (!m_buffer.IsEmpty())
    {
        size_t nLen = m_buffer.ReadableSize();

        BufferSequence  bf;
        m_buffer.GetDatum(bf, nLen);
        assert (nLen == bf.TotalBytes());

        if (!m_flushHook)
        {
            hasData = true;
            for (size_t i = 0; i < bf.count; ++ i)
            {
                Write(bf.buffers[i].iov_base, bf.buffers[i].iov_len);
            }
                
            m_buffer.AdjustReadPtr(nLen);

            return  true;
        }
        else
        {
            AttachedBuffer abf(bf);
            size_t  nWritten = m_flushHook(abf.ReadAddr(), abf.ReadableSize());
            // check level and len, if satisfied, call ::Write(), then return length to skip;
            if (nWritten > 0)
            {
                hasData = true;
                m_buffer.AdjustReadPtr(nWritten);
            }
        }
    }
    else
    {
        if (m_backBytes > 0 && m_backBufLock.TryLock())
        {
            _AssureSpace(m_backBytes);

            if (!m_flushHook)
            {
                hasData = true;

                Write(m_backBuf.ReadAddr(), m_backBytes);
                UnboundedBuffer().Swap(m_backBuf);
                m_backBytes = 0;
            }
            else
            {
                size_t nWritten = m_flushHook(m_backBuf.ReadAddr(), m_backBytes);
                // check level and len, if satisfied, call ::Write(), then return length to skip;
                if (nWritten > 0)
                {
                    hasData = true;
                    m_backBuf.AdjustReadPtr(nWritten);
                    m_backBytes -= nWritten;
                    std::cerr << "flush hook backbuffer " << nWritten << std::endl;
                }
            }

            m_backBufLock.Unlock();
        }
    }

    return  hasData;
}

bool AsyncOutputFile::MakeDir(const char* pDir)
{
    if (mkdir(pDir, 0755) != 0)
    {
        if (EEXIST != errno)
            return false;
    }

    return true;
}

