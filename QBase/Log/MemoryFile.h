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

    int				file_;
    char*           pMemory_;
    std::size_t     offset_;
    std::size_t     size_;
};

template <typename T>
inline T  InputMemoryFile::Read()
{
    T res(*reinterpret_cast<T* >(pMemory_ + offset_));
    offset_ += sizeof(T);
    
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
    
    
    std::size_t Size()   const { return size_;   }
    bool        IsOpen() const;

private:
    int			file_;
    size_t      size_;
};


template <typename T>
inline size_t   OutputMemoryFile::Write(const T& t)
{
    return  this->Write(&t, sizeof t);
}

#endif

