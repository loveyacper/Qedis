
#ifndef BERT_BUFFER_H
#define BERT_BUFFER_H

#include <cassert>
#include <cstring>
#include <vector>
#include <string>
#include <sys/uio.h>
#include <atomic>


struct BufferSequence
{
    static const std::size_t kMaxIovec = 16;

    iovec       buffers[kMaxIovec];
    std::size_t count;

    std::size_t TotalBytes() const
    {
        assert (count <= kMaxIovec);
        std::size_t nBytes = 0;
        for (std::size_t i = 0; i < count; ++ i)
            nBytes += buffers[i].iov_len;

        return nBytes;
    }
};


//static const std::size_t  DEFAULT_BUFFER_SIZE = 4 * 1024;

inline std::size_t RoundUp2Power(std::size_t size)
{
    if (0 == size)  return 0;

    std::size_t roundSize = 1;
    while (roundSize < size)
        roundSize <<= 1;

    return roundSize;
}


template <typename BUFFER>
class  CircularBuffer
{
public:
    // Constructor  to be specialized
    explicit CircularBuffer(std::size_t size = 0) : m_maxSize(size),
    m_readPos(0),
    m_writePos(0)
    {
    }
    CircularBuffer(const BufferSequence& bf);
    CircularBuffer(char* , std::size_t );
   ~CircularBuffer() { }

    bool IsEmpty() const { return m_writePos == m_readPos; }
    bool IsFull()  const { return ((m_writePos + 1) & (m_maxSize - 1)) == m_readPos; }

    // For gather write
    void GetDatum(BufferSequence& buffer, std::size_t maxSize, std::size_t offset = 0);

    // For scatter read
    void GetSpace(BufferSequence& buffer, std::size_t offset = 0);

    // Put data into internal m_buffer
    bool PushData(const void* pData, std::size_t nSize);
    bool PushDataAt(const void* pData, std::size_t nSize, std::size_t offset = 0);

    // Get data from internal m_buffer, adjust read ptr
    bool PeekData(void* pBuf, std::size_t nSize);
    bool PeekDataAt(void* pBuf, std::size_t nSize, std::size_t offset = 0);

    char* ReadAddr() { return &m_buffer[m_readPos]; }
    char* WriteAddr() { return &m_buffer[m_writePos]; }

    void AdjustWritePtr(std::size_t size);
    void AdjustReadPtr(std::size_t size);

    std::size_t ReadableSize()  const {  return (m_writePos - m_readPos) & (m_maxSize - 1);  }
    std::size_t WritableSize()  const {  return m_maxSize - ReadableSize() - 1;  }

    void Clear() {  AtomicSet(&m_readPos, m_writePos);  }

    std::size_t Capacity() const { return m_maxSize; }
    void InitCapacity(std::size_t size);

    template <typename T>
    CircularBuffer& operator<< (const T& data);
    template <typename T>
    CircularBuffer& operator>> (T& data);

    template <typename T>
    CircularBuffer & operator<< (const std::vector<T>& );
    template <typename T>
    CircularBuffer & operator>> (std::vector<T>& );

    CircularBuffer & operator<< (const std::string& str);
    CircularBuffer & operator>> (std::string& str);

protected:
    // The max capacity of m_buffer
    std::size_t m_maxSize;

private:
    // The starting address can be read
    std::atomic<std::size_t> m_readPos;

    // The starting address can be write
    std::atomic<std::size_t> m_writePos;

    // The real internal buffer
    BUFFER m_buffer;

    bool   m_owned = false;
};

template <typename BUFFER>
void CircularBuffer<BUFFER>::GetDatum(BufferSequence& buffer, std::size_t maxSize, std::size_t offset)
{
    if (maxSize == 0 ||
        offset >= ReadableSize()
       )
    {
        buffer.count = 0;
        return;
    }

    assert(m_readPos  < m_maxSize);
    assert(m_writePos < m_maxSize);

    std::size_t   bufferIndex  = 0;
    const std::size_t readPos  = (m_readPos + offset) & (m_maxSize - 1);
    const std::size_t writePos = m_writePos;
    assert (readPos != writePos);

    buffer.buffers[bufferIndex].iov_base = &m_buffer[readPos];
    if (readPos < writePos)
    {            
        if (maxSize < writePos - readPos)
            buffer.buffers[bufferIndex].iov_len = maxSize;
        else
            buffer.buffers[bufferIndex].iov_len = writePos - readPos;
    }
    else
    {
        std::size_t nLeft = maxSize;
        if (nLeft > (m_maxSize - readPos))
            nLeft = (m_maxSize - readPos);
        buffer.buffers[bufferIndex].iov_len = nLeft;
        nLeft = maxSize - nLeft;
 
        if (nLeft > 0 && writePos > 0)
        {
            if (nLeft > writePos)
                nLeft = writePos;

            ++ bufferIndex;
            buffer.buffers[bufferIndex].iov_base = &m_buffer[0];
            buffer.buffers[bufferIndex].iov_len = nLeft;
        }
    }

    buffer.count = bufferIndex + 1;
}

template <typename BUFFER>
void CircularBuffer<BUFFER>::GetSpace(BufferSequence& buffer, std::size_t offset)
{
    assert(m_readPos >= 0 && m_readPos < m_maxSize);
    assert(m_writePos >= 0 && m_writePos < m_maxSize);

    if (WritableSize() <= offset + 1)
    {
        buffer.count = 0;
        return;
    }

    std::size_t bufferIndex = 0;
    const std::size_t readPos  = m_readPos;
    const std::size_t writePos = (m_writePos + offset) & (m_maxSize - 1);

    buffer.buffers[bufferIndex].iov_base = &m_buffer[writePos];

    if (readPos > writePos)
    {
        buffer.buffers[bufferIndex].iov_len = readPos - writePos - 1;
        assert (buffer.buffers[bufferIndex].iov_len > 0);
    }
    else
    {
        buffer.buffers[bufferIndex].iov_len = m_maxSize - writePos;
        if (0 == readPos)
        {
            buffer.buffers[bufferIndex].iov_len -= 1;
        }
        else if (readPos > 1) // 缓冲区必须大于0
        {
            ++ bufferIndex;
            buffer.buffers[bufferIndex].iov_base = &m_buffer[0];
            buffer.buffers[bufferIndex].iov_len = readPos - 1;
        }
    }

    buffer.count = bufferIndex + 1;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PushDataAt(const void* pData, std::size_t nSize, std::size_t offset)
{
    if (!pData || 0 == nSize)
        return true;

    if (offset + nSize > WritableSize())
        return false;

    const std::size_t readPos = m_readPos;
    const std::size_t writePos = (m_writePos + offset) & (m_maxSize - 1);
    if (readPos > writePos)
    {
        assert(readPos - writePos > nSize);
        ::memcpy(&m_buffer[writePos], pData, nSize);
    }
    else
    {
        std::size_t availBytes1 = m_maxSize - writePos;
        std::size_t availBytes2 = readPos - 0;
        assert (availBytes1 + availBytes2 >= 1 + nSize);

        if (availBytes1 >= nSize + 1)
        {
            ::memcpy(&m_buffer[writePos], pData, nSize);
        }
        else
        {
            ::memcpy(&m_buffer[writePos], pData, availBytes1);
            int bytesLeft = static_cast<int>(nSize - availBytes1);
            if (bytesLeft > 0)
                ::memcpy(&m_buffer[0], static_cast<const char*>(pData) + availBytes1, bytesLeft);
        }
    }

    return  true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PushData(const void* pData, std::size_t nSize)
{
    if (!PushDataAt(pData, nSize))
        return false;

    AdjustWritePtr(nSize);
    return true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PeekDataAt(void* pBuf, std::size_t nSize, std::size_t offset)
{
    if (!pBuf || 0 == nSize)
        return true;

    if (nSize + offset > ReadableSize())
        return false;

    const std::size_t writePos = m_writePos;
    const std::size_t readPos  = (m_readPos + offset) & (m_maxSize - 1);
    if (readPos < writePos)
    {
        assert(writePos - readPos >= nSize);
        ::memcpy(pBuf, &m_buffer[readPos], nSize);
    }
    else
    {
        assert(readPos > writePos);
        std::size_t availBytes1 = m_maxSize - readPos;
        std::size_t availBytes2 = writePos - 0;
        assert(availBytes1 + availBytes2 >= nSize);

        if (availBytes1 >= nSize)
        {
            ::memcpy(pBuf, &m_buffer[readPos], nSize);            
        }
        else
        {
            ::memcpy(pBuf, &m_buffer[readPos], availBytes1);
            assert(nSize - availBytes1 > 0);
            ::memcpy(static_cast<char*>(pBuf) + availBytes1, &m_buffer[0], nSize - availBytes1);
        }
    }

    return true;
}

template <typename BUFFER>
bool CircularBuffer<BUFFER>::PeekData(void* pBuf, std::size_t nSize)
{
    if (PeekDataAt(pBuf, nSize))
        AdjustReadPtr(nSize);
    else 
        return false;

    return true;
}


template <typename BUFFER>
inline void CircularBuffer<BUFFER>::AdjustWritePtr(std::size_t size)
{
    std::size_t writePos = m_writePos;
    writePos += size;
    writePos &= m_maxSize - 1;
    
    m_writePos = writePos;
}

template <typename BUFFER>
inline void CircularBuffer<BUFFER>::AdjustReadPtr(std::size_t size)
{
    std::size_t readPos = m_readPos;
    readPos += size;
    readPos &= m_maxSize - 1;
    
    m_readPos = readPos;
}

template <typename BUFFER>
inline void CircularBuffer<BUFFER>::InitCapacity(std::size_t size)
{
    assert (size > 0 && size <= 1 * 1024 * 1024 * 1024);

    m_maxSize = RoundUp2Power(size);
    m_buffer.resize(m_maxSize);
    std::vector<char>(m_buffer).swap(m_buffer);
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator<< (const T& data )
{
    if (!PushData(&data, sizeof(data)))
        assert (!!!"Please modify the DEFAULT_BUFFER_SIZE");

    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (T& data )
{
    if (!PeekData(&data, sizeof(data)))
        assert(!!!"Not enough data in m_buffer");

    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator<< (const std::vector<T>& v)
{
    if (!v.empty())
    {
        (*this) << static_cast<unsigned short>(v.size());
        for ( typename std::vector<T>::const_iterator it = v.begin(); it != v.end(); ++it )
        {
            (*this) << *it;
        } 
    }
    return *this;
}

template <typename BUFFER>
template <typename T>
inline CircularBuffer<BUFFER> & CircularBuffer<BUFFER>::operator>> (std::vector<T>& v)
{
    v.clear();
    unsigned short size;
    *this >> size;
    v.reserve(size);

    while (size--)
    {
        T t;
        *this >> t;
        v.push_back(t);
    }
    return *this;
}

template <typename BUFFER>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator<< (const std::string& str)
{
    *this << static_cast<unsigned short>(str.size());
    if (!PushData(str.data(), str.size()))
    {
        AdjustWritePtr(static_cast<int>(0 - sizeof(unsigned short)));
        assert(!!!"2Please modify the DEFAULT_BUFFER_SIZE");
    }
    return *this;
}

template <typename BUFFER>
inline CircularBuffer<BUFFER>& CircularBuffer<BUFFER>::operator>> (std::string& str)
{
    unsigned short size = 0;
    *this >> size;
    str.clear();
    str.reserve(size);

    char ch;
    while ( size-- )
    {
        *this >> ch;
        str += ch;
    }
    return *this;
}

///////////////////////////////////////////////////////////////
typedef CircularBuffer< ::std::vector<char> >  Buffer;

template <>
inline Buffer::CircularBuffer(std::size_t maxSize) : m_maxSize(RoundUp2Power(maxSize)), m_readPos(0), m_writePos(0), m_buffer(m_maxSize)
{
    assert (0 == (m_maxSize & (m_maxSize - 1)) && "m_maxSize MUST BE power of 2");
}


template <int N>
class StackBuffer : public CircularBuffer<char [N]>
{
    using CircularBuffer<char [N]>::m_maxSize;
public:
    StackBuffer()
    {
        m_maxSize = N;
        if (m_maxSize < 0)
            m_maxSize = 1;

        if (0 != (m_maxSize & (m_maxSize - 1)))
            m_maxSize = RoundUp2Power(m_maxSize);

        assert (0 == (m_maxSize & (m_maxSize - 1)) && "m_maxSize MUST BE power of 2");
    }
};

typedef CircularBuffer<char* > AttachedBuffer;

template <>
inline AttachedBuffer::CircularBuffer(char* pBuf, std::size_t len) : m_maxSize(RoundUp2Power(len + 1)),
                                                             m_readPos(0),
                                                             m_writePos(0)
{
    m_buffer = pBuf;
    m_owned  = false;
}

template <>
inline AttachedBuffer::CircularBuffer(const BufferSequence& bf) :
m_readPos(0),
m_writePos(0)
{
    m_owned = false;

    if (0 == bf.count)
    {
        m_buffer = 0;
    }
    else if (1 == bf.count)
    {
        m_buffer   = (char*)bf.buffers[0].iov_base;
        m_writePos = static_cast<int>(bf.buffers[0].iov_len);
    }
    else if (bf.count > 1)
    {
        m_owned  = true;
        m_buffer = new char[bf.TotalBytes()];

        std::size_t off = 0;
        for (std::size_t i = 0; i < bf.count; ++ i)
        {
            memcpy(m_buffer + off, bf.buffers[i].iov_base, bf.buffers[i].iov_len);
            off += bf.buffers[i].iov_len;
        }

        m_writePos = bf.TotalBytes();
    }

    m_maxSize = RoundUp2Power(m_writePos - m_readPos + 1);
}

template <>
inline AttachedBuffer::~CircularBuffer()
{
    if (m_owned)
        delete [] m_buffer;
}

template <typename T>
inline void OverwriteAt(void* addr, T data)
{
	memcpy(addr, &data, sizeof(data));
}

#endif

