#ifndef BERT_MEMORYFILE_H
#define BERT_MEMORYFILE_H

#include <string>

class MemoryFile
{
public:
    MemoryFile();
   ~MemoryFile();

    bool        Open(const std::string& file, bool bAppend = true);
    bool        Open(const char* file, bool bAppend = true);
    bool        OpenForRead(const char* file);
    void        Close();
    bool        Sync();

    void        Write(const void* data, std::size_t len);
    template <typename T>
    size_t      Write(const T& t);
    
    const char* Read(std::size_t& len);
    void        Skip(std::size_t len);
    
    template <typename T>
    T           Read();

    bool        IsOpen() const { return m_file != kInvalidFile; }
    std::size_t Offset() const { return m_offset; }

    static bool MakeDir(const char* pDir);

    static const int   kInvalidFile = -1;
    static char* const kInvalidAddr;

private:
    bool            _MapReadOnly();
    bool            _ExtendFileSize(std::size_t  size);
    bool            _AssureSpace(std::size_t  size);

    int				m_file;
    char*           m_pMemory;
    std::size_t     m_offset;
    std::size_t     m_size;
    
    std::size_t     m_syncPos;
};


template <typename T>
inline size_t   MemoryFile::Write(const T& t)
{
    this->Write(&t, sizeof t);
    return  sizeof t;
}

template <typename T>
inline T  MemoryFile::Read()
{
    T res(*reinterpret_cast<T* >(m_pMemory + m_offset));
    m_offset += sizeof(T);
   // assert (m_offset <= m_size);
    
    return res;
}

#endif

