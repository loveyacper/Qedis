#include <cassert>
#include <iostream>
#include <set>
#include "QGlobRegex.h"

QGlobRegex::QGlobRegex(const char* pattern, std::size_t plen,
                       const char* text, std::size_t tlen)
{
    SetPattern(pattern, plen);
    SetText(text, tlen);
}

void QGlobRegex::SetPattern(const char* pattern, std::size_t plen)
{
    m_pattern = pattern;
    m_pLen = plen;
    m_pOff = 0;
}

void QGlobRegex::SetText(const char* text, std::size_t tlen)
{
    m_text = text;
    m_tLen = tlen;
    m_tOff = 0;
}

bool QGlobRegex::TryMatch()
{
    while (m_pOff < m_pLen)
    {
        switch (m_pattern[m_pOff])
        {
        case '*':
            return  _ProcessStar();

        case '?':
            if (!_ProcessQuestion())
                return false;

            break;

        case '[':
            if (!_ProcessBracket())
                return false;

            break;

        case '\\':
            if (m_pOff + 1 < m_pLen &&
                m_pattern[m_pOff + 1] == '[')
                ++ m_pOff;
            // fall through;

        default:
            if (m_pattern[m_pOff] != m_text[m_tOff])
                return false;

            ++ m_pOff;
            ++ m_tOff;
            break;
        }
    }
    
    return _IsMatch();
}

bool QGlobRegex::_ProcessStar()
{
    assert(m_pattern[m_pOff] == '*');

    do
    {
        ++ m_pOff;
    } while (m_pOff < m_pLen && m_pattern[m_pOff] == '*');

    if (m_pOff == m_pLen)
        return  true;

    while (m_tOff < m_tLen)
    {
        std::size_t oldpoff = m_pOff;
        if (TryMatch())
        {
            return true;
        }

        m_pOff = oldpoff;
        ++ m_tOff;
    }

    return false;
}

bool QGlobRegex::_ProcessQuestion()
{
    assert(m_pattern[m_pOff] == '?');

    while (m_pOff < m_pLen)
    {
        if (m_pattern[m_pOff] != '?')
            break;

        if (m_tOff == m_tLen)
        {
            return false; // str is too short
        }

        ++ m_pOff;
        ++ m_tOff;
    }

    return true;
}

            
bool  QGlobRegex::_ProcessBracket()
{
    assert(m_pattern[m_pOff] == '[');

    if (m_pOff + 1 >= m_pLen)
    {
        std::cerr << "expect ] at end\n";
        return  false;
    }

    ++ m_pOff;

    bool  include = true;
    if (m_pattern[m_pOff] == '^')
    {
        include = false;
        ++ m_pOff;
    }

    std::set<char>  chars;
    
    if (m_pOff < m_pLen && m_pattern[m_pOff] == ']')
    {
        chars.insert(']'); // No allowed empty brackets.
        ++ m_pOff;
    }

    std::set<std::pair<int, int> >  spans;
    while (m_pOff < m_pLen && m_pattern[m_pOff] != ']')
    {
        if ((m_pOff + 3) < m_pLen && m_pattern[m_pOff + 1] == '-')
        {
            int start = m_pattern[m_pOff];
            int end   = m_pattern[m_pOff + 2];

            if (start == end)
            {
                chars.insert(start);
            }
            else
            {
                if (start > end)
                    std::swap(start, end);

                spans.insert(std::make_pair(start, end));
            }

            m_pOff += 3;
        }
        else 
        {
            chars.insert(m_pattern[m_pOff]);
            ++ m_pOff;
        }
    }

    if (m_pOff == m_pLen)
    {
        std::cerr << "expect ]\n";
        return  false;
    }
    else
    {
        assert (m_pattern[m_pOff] == ']');
        ++ m_pOff;
    }

    if (chars.count(m_text[m_tOff]) > 0)
    {
        if (include)
        {
            ++ m_tOff;
            return true;
        }
        else
        {
            return false;
        }
    }

    for (std::set<std::pair<int, int> >::const_iterator it(spans.begin());
            it != spans.end();
            ++ it)
    {
        if (m_text[m_tOff] >= it->first && m_text[m_tOff] <= it->second)
        {
            if (include)
            {
                ++ m_tOff;
                return true;
            }
            else
            {
                return false;
            }
        }
    }

    if (include)
    {
        std::cerr << "include but not match " << m_text[m_tOff] << std::endl;
        return false;
    }
    else
    {
        std::cerr << "not include and not match " << m_text[m_tOff] << std::endl;
        ++ m_tOff;
        return true;
    }
}

bool QGlobRegex::_IsMatch() const
{
    if (m_pOff != m_pLen)
        std::cerr << "poff = " << m_pOff << " but plen = " << m_pLen << std::endl;
    if (m_tOff != m_tLen)
        std::cerr << "toff = " << m_tOff << " but tlen = " << m_tLen << std::endl;

    return m_pOff == m_pLen && m_tLen == m_tOff;
}
