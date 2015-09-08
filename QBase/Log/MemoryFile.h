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

    size_t      Write(const void* data, std::size_t len);
    template <typename T>
    size_t      Write(const T& t);
    
    
    std::size_t Size()   const { return m_size;   }
    bool        IsOpen() const;

private:
    int			m_file;
    size_t      m_size;
};


template <typename T>
inline size_t   OutputMemoryFile::Write(const T& t)
{
    return  this->Write(&t, sizeof t);
}

#endif

