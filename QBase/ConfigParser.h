#ifndef BERT_CONFIGPARSER_H
#define BERT_CONFIGPARSER_H

#include <map>
#include <string>
#include <sstream>

#ifdef CONFIG_DEBUG
#include <iostream>
#endif

class ConfigParser
{
public:
    bool Load(const char* FileName);
#if 0
    bool Save(const char* FileName);
    bool Save(const char* FileName, const std::string& key, const std::string& value);
#endif

    template <typename T>
    T   GetData(const char* key, const T& default_ = T()) const;

    typedef std::map<std::string, std::string>::iterator iterator;
    iterator  begin() { return m_data.begin();  }
    iterator  end()   { return m_data.end();    }

    void    clear() { m_data.clear(); }
    bool    insert(const std::string& key, const std::string& val)
    {
        return  m_data.insert(std::pair<std::string, std::string>(key, val)).second;
    }

#ifdef CONFIG_DEBUG
    void Print()
    {
        std::cout << "//////////////////"<< std::endl;
        std::map<std::string, std::string>::const_iterator it = m_data.begin();
        while (it != m_data.end())
        {
            std::cout << it->first << ":" << it->second << "\n";
            ++ it;
        }
    }
#endif

private:
    std::map<std::string, std::string> m_data;

    template <typename T>
    T  _ToType(const std::string& data) const;
};


template <typename T>
inline  T  ConfigParser::_ToType(const std::string& data) const
{
    T        t;
    std::istringstream  os(data);
    os >> t;
    return  t;
}

template <>
inline  const char*  ConfigParser::_ToType<const char* >(const std::string& data) const
{
    return  data.c_str();
}

template <>
inline  std::string  ConfigParser::_ToType<std::string >(const std::string& data) const
{
    return  data;
}


template <typename T>
inline  T  ConfigParser::GetData(const char* key, const T& default_) const
{
    std::map<std::string, std::string>::const_iterator it = m_data.find(key);
    if (it == m_data.end())
        return default_;

    return  _ToType<T>(it->second);
}

#endif

