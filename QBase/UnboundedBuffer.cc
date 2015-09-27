
#include "UnboundedBuffer.h"
#include <iostream>
#include <limits>
#include <cassert>
    
const std::size_t UnboundedBuffer::MAX_BUFFER_SIZE = std::numeric_limits<std::size_t>::max() / 2;


std::size_t UnboundedBuffer::Write(const void* pData, std::size_t nSize)
{
    return PushData(pData, nSize);
}

std::size_t UnboundedBuffer::PushData(const void* pData, std::size_t nSize)
{
    std::size_t nBytes  = PushDataAt(pData, nSize);
    AdjustWritePtr(nBytes);

    return nBytes;
}

std::size_t UnboundedBuffer::PushDataAt(const void* pData, std::size_t nSize, std::size_t offset)
{
    if (!pData || nSize == 0)
        return 0;

    if (ReadableSize() == UnboundedBuffer::MAX_BUFFER_SIZE)
        return 0;

    _AssureSpace(nSize + offset);

    assert (nSize + offset <= WritableSize());

   	::memcpy(&m_buffer[m_writePos + offset], pData, nSize);
    return  nSize;
}

std::size_t UnboundedBuffer::PeekData(void* pBuf, std::size_t nSize)
{
    std::size_t nBytes  = PeekDataAt(pBuf, nSize);
    AdjustReadPtr(nBytes);

    return nBytes;
}

std::size_t UnboundedBuffer::PeekDataAt(void* pBuf, std::size_t nSize, std::size_t offset)
{
    const std::size_t dataSize = ReadableSize();
    if (!pBuf ||
         nSize == 0 ||
         dataSize <= offset)
        return 0;

    if (nSize + offset > dataSize)
        nSize = dataSize - offset;

	::memcpy(pBuf, &m_buffer[m_readPos + offset], nSize);

    return nSize;
}


void UnboundedBuffer::_AssureSpace(std::size_t nSize)
{
    if (nSize <= WritableSize())
        return;

    std::size_t maxSize = m_buffer.size();

    while (nSize > WritableSize() + m_readPos)
    {
        if (maxSize == 0)
            maxSize = 1;
        else if (maxSize <= UnboundedBuffer::MAX_BUFFER_SIZE)
            maxSize <<= 1;
        else 
            break;

        m_buffer.resize(maxSize);
    }
        
    if (m_readPos > 0)
    {
        std::size_t dataSize = ReadableSize();
        std::cout << dataSize << " bytes moved from " << m_readPos << std::endl;
        ::memmove(&m_buffer[0], &m_buffer[m_readPos], dataSize);
        m_readPos  = 0;
        m_writePos = dataSize;
    }
}

void UnboundedBuffer::Shrink(bool tight)
{
    assert (m_buffer.capacity() == m_buffer.size());

    if (m_buffer.empty())
    { 
        assert (m_readPos == 0);
        assert (m_writePos == 0);
        return;
    }

    std::size_t oldCap   = m_buffer.size();
    std::size_t dataSize = ReadableSize();
    if (!tight && dataSize > oldCap / 2)
        return;

    std::vector<char>  tmp;
    tmp.resize(dataSize);
    memcpy(&tmp[0], &m_buffer[m_readPos], dataSize);
    tmp.swap(m_buffer);

    m_readPos  = 0;
    m_writePos = dataSize;

    std::cout << oldCap << " shrink to " << m_buffer.size() << std::endl;
}

void UnboundedBuffer::Clear()
{
    m_readPos = m_writePos = 0; 
}


void UnboundedBuffer::Swap(UnboundedBuffer& buf)
{
    m_buffer.swap(buf.m_buffer);
    std::swap(m_readPos, buf.m_readPos);
    std::swap(m_writePos, buf.m_writePos);
}

#if 0
int main()
{
    UnboundedBuffer    buf;
    std::size_t ret = buf.PushData("hello", 5);
    assert (ret == 5);

    char tmp[10];
    ret = buf.PeekData(tmp, sizeof tmp);
    assert(ret == 5);
    assert(tmp[0] == 'h');

    assert(buf.IsEmpty());

    ret = buf.PushData("world", 5);
    assert (ret == 5);
    ret = buf.PushData("abcde", 5);
    assert (ret == 5);
    ret = buf.PeekData(tmp, 5);
    assert(tmp[0] == 'w');

    buf.Clear();
    buf.Shrink();

#if 1
    ret = buf.PeekData(tmp, 5);
    if (ret == 5)
    {
        assert(tmp[0] == 'a');
        assert(tmp[1] == 'b');
    }
#endif
    buf.Shrink();

    return 0;
}
#endif

