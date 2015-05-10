#ifndef BERT_MEMORYFILE_H
#define BERT_MEMORYFILE_H

#include <string>
    
class InputMemoryFile
{
public:
    InputMemoryFile();
   ~InputMemoryFile();

    bool        Open(const char* file);
    void        Close();

    const char* Read(std::size_t& len);
    void        Skip(std::size_t len);
    
    template <typename T>
    T           Read();

    std::size_t Offset() const { return m_offset; }
    bool        IsOpen() const;

private:
    bool            _MapReadOnly();

    int				m_file;
    char*           m_pMemory;
    std::size_t     m_offset;
    std::size_t     m_size;
};

template <typename T>
inline T  InputMemoryFile::Read()
{
    T res(*reinterpret_cast<T* >(m_pMemory + m_offset));
    m_offset += sizeof(T);
   // assert (m_offset <= m_size);
    
    return res;
}


class OutputMemoryFile
{
public:
    OutputMemoryFile();
   ~OutputMemoryFile();

    bool        Open(const std::string& file, bool bAppend = true);
    bool        Open(const char* file, bool bAppend = true);
    void        Close();
    bool        Sync();

    void        Truncate(std::size_t size);

    //!! if process terminated abnormally, erase the trash data
    void        TruncateTailZero();

    void        Write(const void* data, std::size_t len);
    template <typename T>
    size_t      Write(const T& t);
    
    std::size_t Offset() const { return m_offset; }
    bool        IsOpen() const;

private:
    bool            _MapWriteOnly();
    void            _ExtendFileSize(std::size_t  size);
    void            _AssureSpace(std::size_t  size);

    int				m_file;
    char*           m_pMemory;
    std::size_t     m_offset;
    std::size_t     m_size;
    
    std::size_t     m_syncPos;
};


template <typename T>
inline size_t   OutputMemoryFile::Write(const T& t)
{
    this->Write(&t, sizeof t);
    return  sizeof t;
}

#endif

