#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>
#include <unistd.h>

using namespace std;

#include "MemoryFile.h"

using std::size_t;
    
static const int   kInvalidFile = -1;
static char* const kInvalidAddr = reinterpret_cast<char*>(-1);

InputMemoryFile::InputMemoryFile() : m_file(kInvalidFile),
                                     m_pMemory(kInvalidAddr),
                                     m_offset(0),
                                     m_size(0)
{
}

InputMemoryFile::~InputMemoryFile()
{
    Close();
}

bool InputMemoryFile::_MapReadOnly()
{
    assert (m_file != kInvalidFile);
    assert (m_size == 0);

    struct stat st;
    fstat(m_file, &st);
    m_size = st.st_size;

    m_pMemory = (char* )::mmap(0, m_size, PROT_READ, MAP_SHARED, m_file, 0);
    return   m_pMemory != kInvalidAddr;
}

bool  InputMemoryFile::Open(const char* file)
{
    Close();

    m_file = ::open(file, O_RDONLY);

    if (m_file == kInvalidFile)
    {
        char err[128];
        snprintf(err, sizeof err - 1, "OpenForRead %s failed\n", file);
        perror(err);
        return false;
    }

    m_offset = 0;
    return _MapReadOnly();
}

void  InputMemoryFile::Close()
{
    if (m_file != kInvalidFile)
    {
        ::munmap(m_pMemory, m_size);
        ::close(m_file);

        m_file      = kInvalidFile;
        m_size      = 0;
        m_pMemory   = kInvalidAddr;
        m_offset    = 0;
    }
}

const char* InputMemoryFile::Read(std::size_t& len)
{
    if (m_size <= m_offset)
        return 0;

    if (m_offset + len > m_size)
        len = m_size - m_offset;

    return  m_pMemory + m_offset;
}

void InputMemoryFile::Skip(size_t len)
{
    m_offset += len;
    assert (m_offset <= m_size);
}

bool InputMemoryFile::IsOpen() const
{
    return  m_file != kInvalidFile;
}


// OutputMemoryFile

OutputMemoryFile::OutputMemoryFile() : m_file(kInvalidFile),
                                       m_pMemory(kInvalidAddr),
                                       m_offset(0),
                                       m_size(0),
                                       m_syncPos(0)
{
}

OutputMemoryFile::~OutputMemoryFile()
{
    Close();
}

void OutputMemoryFile::_ExtendFileSize(size_t  size)
{
    assert (m_file != kInvalidFile);

    if (size > m_size)
        Truncate(size);
}

bool  OutputMemoryFile::Open(const std::string&  file, bool bAppend)
{
    return Open(file.c_str(), bAppend);
}

bool  OutputMemoryFile::Open(const char* file, bool bAppend)
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

    struct stat st;
    fstat(m_file, &st);
    m_size = st.st_size;
    cerr << "open size " << m_size << endl;

    if (bAppend)
    {
        m_offset = m_size;
    }
    else
    {
        ::ftruncate(m_file, 1 * 1024 * 1024);
        m_size    = 1 * 1024 * 1024;
        m_offset  = 0;
    }
    
    return _MapWriteOnly();
}

void  OutputMemoryFile::Close()
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
        m_syncPos   = 0;
    }
}

bool    OutputMemoryFile::Sync()
{
    if (m_syncPos >= m_offset)
        return false;
    
    ::msync(m_pMemory + m_syncPos, m_offset - m_syncPos, MS_SYNC);
    m_syncPos = m_offset;
    return true;
}

bool   OutputMemoryFile::_MapWriteOnly()
{
    if (m_size == 0 || m_file == kInvalidFile)
        return false;
        
    if (m_pMemory != kInvalidAddr)
        ::munmap(m_pMemory, m_size);

    m_pMemory = (char* )::mmap(0, m_size, PROT_WRITE, MAP_SHARED, m_file, 0);
    return (m_pMemory != kInvalidAddr);
}

void    OutputMemoryFile::Truncate(std::size_t  size)
{
    if (size == m_size)
        return;
    
    m_size = size;
    ::ftruncate(m_file, size);
    
    if (m_offset > m_size)
        m_offset = m_size;
    
    _MapWriteOnly();
}

void    OutputMemoryFile::TruncateTailZero()
{
    if (m_file == kInvalidFile)
        return;

    size_t  tail = m_size;
    while (tail > 0 && m_pMemory[--tail] == '\0')
        ;

    ++ tail;

    cerr << "tail " << tail << endl;;

    Truncate(tail);
}

bool  OutputMemoryFile::IsOpen() const
{
    return  m_file != kInvalidFile;
}

// consumer
void   OutputMemoryFile::Write(const void* data, size_t len)
{
    _AssureSpace(len);
        
    ::memcpy(m_pMemory + m_offset, data, len);
    m_offset += len;
    assert(m_offset <= m_size);
}
    
void   OutputMemoryFile::_AssureSpace(size_t  size)
{
    size_t  newSize = m_size;
    while (m_offset + size > newSize)
    {
        if (newSize == 0)
            newSize = 1 * 1024 * 1024;
        else
            newSize <<= 1;
    }

    _ExtendFileSize(newSize);
}

