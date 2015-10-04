#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>


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

    m_pMemory = (char* )::mmap(0, m_size, PROT_READ, MAP_PRIVATE, m_file, 0);
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
                                       m_size(0)
{
}

OutputMemoryFile::~OutputMemoryFile()
{
    Close();
}

bool  OutputMemoryFile::Open(const std::string&  file, bool bAppend)
{
    return Open(file.c_str(), bAppend);
}

bool  OutputMemoryFile::Open(const char* file, bool bAppend)
{
    Close();

    m_file = ::open(file, O_RDWR | O_CREAT | (bAppend ? O_APPEND : 0), 0644);

    if (m_file == kInvalidFile)
    {
        char err[128];
        snprintf(err, sizeof err - 1, "OpenWriteOnly %s failed\n", file);
        perror(err);
        return false;
    }
    
    m_size = 0;
    if (bAppend)
    {
        struct stat st;
        fstat(m_file, &st);
        m_size = st.st_size;
    }
    
    return  true;
}

void  OutputMemoryFile::Close()
{
    if (m_file != kInvalidFile)
    {
        ::close(m_file);

        m_file      = kInvalidFile;
        m_size      = 0;
    }
}

bool    OutputMemoryFile::Sync()
{
    if (m_file == kInvalidFile)
        return false;
    
    return 0 == ::fsync(m_file);
}


bool  OutputMemoryFile::IsOpen() const
{
    return  m_file != kInvalidFile;
}

// consumer
size_t   OutputMemoryFile::Write(const void* data, size_t len)
{
    auto n = ::write(m_file, data, len);
    if (n > 0)
        m_size += n;
    else if (n < 0)
        n = 0;
    
    return n;
}

