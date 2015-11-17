#include <vector>
#include "ConfigParser.h"
#include "Log/MemoryFile.h"

static const int  SPACE     = ' ';
static const int  TAB       = '\t';
static const int  NEWLINE   = '\n';
static const int  COMMENT   = '#';


static size_t  SkipBlank(const char* data, size_t len, size_t off)
{
    while (++ off < len)
    {
        if (SPACE != data[off] && TAB != data[off])
        {
            -- off;
            break;
        }
    }

    return off;
}

bool ConfigParser::Load(const char* FileName)
{
    InputMemoryFile  file;
    if (!file.Open(FileName))
        return false; // no such file

    m_data.clear();

    size_t      maxLen = size_t(-1);
    const char* data = file.Read(maxLen);

    bool  bReadKey = true;
    std::string  key, value;
    key.reserve(64);
    value.reserve(64);

    size_t  off = 0;
    while (off < maxLen)
    {
        switch (data[off])
        {
            case COMMENT:
                while (++ off < maxLen)
                {
                    if (NEWLINE == data[off])
                    {
                        -- off;
                        break;
                    }
                }
                break;

            case NEWLINE:
                bReadKey = true;

                if (!key.empty())
                {
                    if (m_data.count(key) > 0)
                    {
                        // duplicate key
                        m_data[key] += " " + value;
                    }
                    else
                    {
                        m_data[key] = value;
                    }
                    
                    key.clear();
                    value.clear();
                }
                
                off = SkipBlank(data, maxLen, off);
                break;
            
            case SPACE:
            case TAB:
                // 支持value中有空格
                if (bReadKey)
                {
                    bReadKey = false;
                    off = SkipBlank(data, maxLen, off); // 跳过所有分界空格
                }
                else
                {
                    value += data[off];
                }
                break;
            
            case '\r':
                break;
            
            default:
                if (bReadKey)
                    key += data[off];
                else
                    value += data[off];

                break;
        }

        ++ off;
    }
    
    file.Close();
    return true;
}

#ifdef CONFIG_DEBUG
int main()
{
    ConfigParser   csv;
    csv.Load("config");
    csv.Print();
	
    std::cout << "=====================" << std::endl;
}
#endif

