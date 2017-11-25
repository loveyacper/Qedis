#include <vector>
#include <fstream>
#include "ConfigParser.h"

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

bool ConfigParser::Load(const char* fileName)
{
    std::ifstream ifs(fileName, std::ifstream::binary);
    if (!ifs)
        return false; // no such file

    std::filebuf* fbuf = ifs.rdbuf();
    const std::size_t size = fbuf->pubseekoff (0, ifs.end, ifs.in);
    fbuf->pubseekpos (0, ifs.in);
      
    // allocate memory to contain file data
    std::unique_ptr<char []> data(new char[size]);
    fbuf->sgetn(data.get(), size);
    ifs.close();

    data_.clear();

    bool bReadKey = true;
    std::string key, value;
    key.reserve(64);
    value.reserve(64);

    size_t off = 0;
    while (off < size)
    {
        switch (data[off])
        {
            case COMMENT:
                while (++ off < size)
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
                    data_[key].push_back(value);
                    
                    key.clear();
                    value.clear();
                }
                
                off = SkipBlank(data.get(), size, off);
                break;
            
            case SPACE:
            case TAB:
                // 支持value中有空格
                if (bReadKey)
                {
                    bReadKey = false;
                    off = SkipBlank(data.get(), size, off); // 跳过所有分界空格
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
    
    return true;
}

const std::vector<std::string>& ConfigParser::GetDataVector(const char* key) const
{
    auto it = data_.find(key);
    if (it == data_.end())
    {
        static const std::vector<std::string> kEmpty;
        return kEmpty;
    }
    
    return it->second;
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

