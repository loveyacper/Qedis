
#ifndef BERT_TIMER_H
#define BERT_TIMER_H

#include <ctime>
#include <sys/time.h>
#include <stdint.h>

class Time
{
public:
    Time();
    Time(const Time& other);
    Time(int hour, int min, int sec); // TODO year month day, and Effective cpp item 18

    void        Now();
    uint64_t    MilliSeconds() const { return m_ms; }
    const char* FormatTime(char* buf, int size) const;
    void        AddDelay(uint64_t delay);

    int GetYear()   const { _UpdateTm(); return m_tm.tm_year + 1900;  } 
    int GetMonth()  const { _UpdateTm(); return m_tm.tm_mon + 1;  } 
    int GetDay()    const { _UpdateTm(); return m_tm.tm_mday; }
    int GetHour()   const { _UpdateTm(); return m_tm.tm_hour; }
    int GetMinute() const { _UpdateTm(); return m_tm.tm_min;  }
    int GetSecond() const { _UpdateTm(); return m_tm.tm_sec;  }

    Time&    operator= (const Time& );
    operator uint64_t() const { return m_ms; }

//private:
    uint64_t    m_ms;      // milliseconds from 1970
    mutable tm  m_tm;
    mutable bool m_valid;
    
    void    _UpdateTm()  const;
};

#endif

