#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <iostream>

#if defined(__APPLE__)
#include <unistd.h>
#endif

#include "MemoryFile.h"

using std::size_t;
    
char* const MemoryFile::kInvalidAddr = reinterpret_cast<char*>(-1);

MemoryFile::MemoryFile() : m_file(kInvalidFile),
                           m_pMemory(kInvalidAddr),
                           m_offset(0),
                           m_size(0),
                           m_syncPos(0)
{
}

MemoryFile::~MemoryFile()
{
    Close();
}

bool MemoryFile::_ExtendFileSize(size_t  size)
{
    assert (m_file != kInvalidFile);

    if (m_size >= size)
        return true;

    if (m_size != 0)
        ::munmap(m_pMemory, m_size);

    ::ftruncate(m_file, size);
    m_size = size;
    m_pMemory = (char* )::mmap(0, size, PROT_WRITE, MAP_SHARED, m_file, 0);

    if (m_pMemory == kInvalidAddr)
        perror("mmap failed ");
    
    return   m_pMemory != kInvalidAddr;
}

bool MemoryFile::_MapReadOnly()
{
    assert (m_file != kInvalidFile);
    assert (m_size == 0);

    struct stat st;
    fstat(m_file, &st);
    m_size = st.st_size;

    m_pMemory = (char* )::mmap(0, m_size, PROT_READ, MAP_SHARED, m_file, 0);

    if (m_pMemory == kInvalidAddr)
        perror("mmap readonly failed ");
    
    return   m_pMemory != kInvalidAddr;
}


bool  MemoryFile::Open(const std::string&  file, bool bAppend)
{
    return Open(file.c_str(), bAppend);
}

bool  MemoryFile::Open(const char* file, bool bAppend)
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

    if (bAppend)
    {
        struct stat st;
        fstat(m_file, &st);
        m_offset  = st.st_size;
    }
    else
    {
        ::ftruncate(m_file, 0);
        m_offset  = 0;
    }
    
    return true;
}

bool  MemoryFile::OpenForRead(const char* file)
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

    return _MapReadOnly();
}

void  MemoryFile::Close()
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

bool    MemoryFile::Sync()
{
    if (m_syncPos >= m_offset)
        return false;
    
    ::msync(m_pMemory + m_syncPos, m_offset - m_syncPos, MS_SYNC);
    m_syncPos = m_offset;
    return true;
}

const char* MemoryFile::Read(std::size_t& len)
{
    if (m_offset + len > m_size)
        len = m_size - m_offset;

    return  m_pMemory + m_offset;
}

void MemoryFile::Skip(size_t len)
{
    m_offset += len;
    assert (m_offset <= m_size);
}

// consumer
void   MemoryFile::Write(const void* data, size_t len)
{
    _AssureSpace(len);
        
    ::memcpy(m_pMemory + m_offset, data, len);
    m_offset += len;
    assert(m_offset <= m_size);
}
    
bool   MemoryFile::_AssureSpace(size_t  size)
{
    size_t  newSize = m_size;
    while (m_offset + size > newSize)
    {
        if (newSize == 0)
            newSize = 1 * 1024 * 1024;
        else
            newSize <<= 1;
    }

    return  _ExtendFileSize(newSize);
}

bool MemoryFile::MakeDir(const char* pDir)
{
    if (mkdir(pDir, 0755) != 0)
    {
        if (EEXIST != errno)
            return false;
    }

    return true;
}


