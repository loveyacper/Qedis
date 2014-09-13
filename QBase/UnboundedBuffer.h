
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

    unsigned PushDataAt(const void* pData, unsigned nSize, unsigned offset = 0);
    unsigned PushData(const void* pData, unsigned nSize);
    void AdjustWritePtr(unsigned nBytes) {   m_writePos += nBytes; }

    unsigned  PeekDataAt(void* pBuf, unsigned nSize, unsigned offset = 0);
    unsigned  PeekData(void* pBuf, unsigned nSize);
    void AdjustReadPtr(unsigned nBytes) {   m_readPos  += nBytes; }

    char* ReadAddr()  {  return &m_buffer[m_readPos];  }
    char* WriteAddr() {  return &m_buffer[m_writePos]; }

    bool IsEmpty() const { return ReadableSize() == 0; }
    unsigned ReadableSize() const {  return m_writePos - m_readPos;  }
    unsigned WritableSize() const {  return static_cast<unsigned>(m_buffer.size()) - m_writePos;  }

    void Shrink(bool tight = false);
    void Clear();

    static const unsigned  MAX_BUFFER_SIZE;
private:
    void     _AssureSpace(unsigned size);
    unsigned m_readPos;
    unsigned m_writePos;
    std::vector<char>  m_buffer;
};


#endif

