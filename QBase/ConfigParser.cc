#include <vector>
#include "ConfigParser.h"

const int ConfigParser::SPACE     = ' ';
const int ConfigParser::TAB       = '\t';
const int ConfigParser::NEWLINE   = '\n';
const int ConfigParser::COMMENT   = '#';


ConfigParser::~ConfigParser()
{
    if (m_file.is_open())
        m_file.close();
}

bool ConfigParser::Load(const char* FileName)
{
    if (m_file.is_open())
        m_file.close();

    m_file.open(FileName, std::ios::in | std::ios::binary);
    if (!m_file)
        return false; // no such file

    m_data.clear();

	bool  bReadKey = true;
    std::string  key, value;
    key.reserve(64);
    value.reserve(64);

    char ch   = 0;
    while (m_file.get(ch))
    {
        switch (ch)
        {
			case COMMENT:
				while (m_file.get(ch))
				{
					if (NEWLINE == ch)
                    {
                        m_file.unget();
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
                        return false;
                    }

					m_data[key] = value;
					key.clear();
					value.clear();
				}

                _SkipBlank();
				break;

            case SPACE:
            case TAB:
                // 支持value中有空格
                if (bReadKey)
                {
                    bReadKey = false;
                    _SkipBlank(); // 跳过所有分界空格
                }
                else
					value += ch;
                break;

            case '\r':
                break;

            default:
				if (bReadKey) 
					key += ch;
				else
					value += ch;

				break;
        }
    }

    m_file.close();
    return true;
}

#if 0
bool ConfigParser::Save(const char* FileName, const std::string& key, const std::string& value)
{
    if (m_file.is_open())
        m_file.close();

    m_file.open(FileName, std::ios::app | std::ios::out | std::ios::binary);

    if (!m_file)
        return false;

    m_file.seekp(0, std::ios_base::end);

    m_file.write(key.data(), key.size());
    m_file.put(TAB);
    m_file.write(value.data(), value.size());
    m_file.put('\n');

    return true;
}

bool ConfigParser::Save(const char* FileName)
{
    if (m_file.is_open())
        m_file.close();

    m_file.open(FileName, std::ios::out | std::ios::binary);

    if (!m_file)
        return false;

    m_file.seekp(0);

    std::map<std::string, std::string>::iterator  it(m_data.begin());
    for (; it != m_data.end(); ++ it)
    {
        m_file.write(it->first.data(), it->first.size());
        m_file.put(TAB);
        m_file.write(it->second.data(), it->second.size());
        m_file.put('\n');
    }

    return true;
}
#endif


void ConfigParser::_SkipBlank()
{
    char    ch;
    while (m_file.get(ch))
    {
        if (SPACE != ch && TAB != ch)
        {
            m_file.unget();
            break;
        }
    }
}

#ifdef CONFIG_DEBUG
int main()
{
	ConfigParser   csv;
	csv.Load("config");
	csv.Print();
    short age = csv.GetData<int>("age");
    
	std::cout << csv.GetData<int>("age") << std::endl;
	std::cout << csv.GetData<const char* >("sex") << std::endl;
	std::cout << csv.GetData<const char* >("self") << std::endl;

	std::cout << "=====================" << std::endl;
}
#endif
