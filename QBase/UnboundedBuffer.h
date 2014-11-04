
#ifndef BERT_UNBOUNDEDBUFFER_H
#define BERT_UNBOUNDEDBUFFER_H

#include <cstring>
#include <vector>

class UnboundedBuffer
{
public:
    UnboundedBuffer() :
        m_readPos(0),
        m_writePos(0)
    {
    }

    std::size_t PushDataAt(const void* pData, std::size_t nSize, std::size_t offset = 0);
    std::size_t PushData(const void* pData, std::size_t nSize);
    void AdjustWritePtr(std::size_t nBytes) {   m_writePos += nBytes; }

    std::size_t  PeekDataAt(void* pBuf, std::size_t nSize, std::size_t offset = 0);
    std::size_t  PeekData(void* pBuf, std::size_t nSize);
    void AdjustReadPtr(std::size_t nBytes) {   m_readPos  += nBytes; }

    char* ReadAddr()  {  return &m_buffer[m_readPos];  }
    char* WriteAddr() {  return &m_buffer[m_writePos]; }

    bool IsEmpty() const { return ReadableSize() == 0; }
    std::size_t ReadableSize() const {  return m_writePos - m_readPos;  }
    std::size_t WritableSize() const {  return m_buffer.size() - m_writePos;  }

    void Shrink(bool tight = false);
    void Clear();
    void Swap(UnboundedBuffer& buf);

    static const std::size_t  MAX_BUFFER_SIZE;
private:
    void     _AssureSpace(std::size_t size);
    std::size_t m_readPos;
    std::size_t m_writePos;
    std::vector<char>  m_buffer;
};


#endif

