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

InputMemoryFile::InputMemoryFile() : file_(kInvalidFile),
                                     pMemory_(kInvalidAddr),
                                     offset_(0),
                                     size_(0)
{
}

InputMemoryFile::~InputMemoryFile()
{
    Close();
}

bool InputMemoryFile::_MapReadOnly()
{
    assert (file_ != kInvalidFile);
    assert (size_ == 0);

    struct stat st;
    fstat(file_, &st);
    size_ = st.st_size;

    pMemory_ = (char* )::mmap(0, size_, PROT_READ, MAP_PRIVATE, file_, 0);
    return   pMemory_ != kInvalidAddr;
}

bool  InputMemoryFile::Open(const char* file)
{
    Close();

    file_ = ::open(file, O_RDONLY);

    if (file_ == kInvalidFile)
    {
        char err[128];
        snprintf(err, sizeof err - 1, "OpenForRead %s failed\n", file);
        //perror(err);
        return false;
    }

    offset_ = 0;
    return _MapReadOnly();
}

void  InputMemoryFile::Close()
{
    if (file_ != kInvalidFile)
    {
        ::munmap(pMemory_, size_);
        ::close(file_);

        file_      = kInvalidFile;
        size_      = 0;
        pMemory_   = kInvalidAddr;
        offset_    = 0;
    }
}

const char* InputMemoryFile::Read(std::size_t& len)
{
    if (size_ <= offset_)
        return 0;

    if (offset_ + len > size_)
        len = size_ - offset_;

    return  pMemory_ + offset_;
}

void InputMemoryFile::Skip(size_t len)
{
    offset_ += len;
    assert (offset_ <= size_);
}

bool InputMemoryFile::IsOpen() const
{
    return  file_ != kInvalidFile;
}


// OutputMemoryFile

OutputMemoryFile::OutputMemoryFile() : file_(kInvalidFile),
                                       size_(0)
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

    file_ = ::open(file, O_RDWR | O_CREAT | (bAppend ? O_APPEND : 0), 0644);

    if (file_ == kInvalidFile)
    {
        char err[128];
        snprintf(err, sizeof err - 1, "OpenWriteOnly %s failed\n", file);
        perror(err);
        return false;
    }
    
    size_ = 0;
    if (bAppend)
    {
        struct stat st;
        fstat(file_, &st);
        size_ = st.st_size;
    }
    
    return  true;
}

void  OutputMemoryFile::Close()
{
    if (file_ != kInvalidFile)
    {
        ::close(file_);

        file_      = kInvalidFile;
        size_      = 0;
    }
}

bool    OutputMemoryFile::Sync()
{
    if (file_ == kInvalidFile)
        return false;
    
    return 0 == ::fsync(file_);
}


bool  OutputMemoryFile::IsOpen() const
{
    return  file_ != kInvalidFile;
}

// consumer
size_t   OutputMemoryFile::Write(const void* data, size_t len)
{
    auto n = ::write(file_, data, len);
    if (n > 0)
        size_ += n;
    else if (n < 0)
        n = 0;
    
    return n;
}

