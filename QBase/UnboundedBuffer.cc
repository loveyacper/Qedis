
#include "UnboundedBuffer.h"
#include <iostream>
#include <limits>
#include <cassert>
    
const unsigned UnboundedBuffer::MAX_BUFFER_SIZE = std::numeric_limits<unsigned >::max() / 2;

unsigned UnboundedBuffer::PushData(const void* pData, unsigned nSize)
{
    unsigned nBytes  = PushDataAt(pData, nSize);
    AdjustWritePtr(nBytes);

    return nBytes;
}

unsigned UnboundedBuffer::PushDataAt(const void* pData, unsigned nSize, unsigned offset)
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

unsigned UnboundedBuffer::PeekData(void* pBuf, unsigned nSize)
{
    unsigned nBytes  = PeekDataAt(pBuf, nSize);
    AdjustReadPtr(nBytes);

    return nBytes;
}

unsigned UnboundedBuffer::PeekDataAt(void* pBuf, unsigned nSize, unsigned offset)
{
    const unsigned dataSize = ReadableSize();
    if (!pBuf ||
         nSize <= 0 ||
         dataSize <= offset)
        return 0;

    if (nSize + offset > dataSize)
        nSize = dataSize - offset;

	::memcpy(pBuf, &m_buffer[m_readPos + offset], nSize);

    return nSize;
}


void UnboundedBuffer::_AssureSpace(unsigned nSize)
{
    if (nSize <= WritableSize())
        return;

    unsigned maxSize = static_cast<unsigned>(m_buffer.size());

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
        std::cout << "Move buffer from " << m_readPos << std::endl;
        unsigned dataSize = ReadableSize();
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

    unsigned oldCap   = static_cast<unsigned>(m_buffer.size());
    unsigned dataSize = ReadableSize();
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

#if 0
int main()
{
    UnboundedBuffer    buf;
    unsigned ret = buf.PushData("hello", 5);
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

