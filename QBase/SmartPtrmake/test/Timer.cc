#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <iostream>
#include "Timer.h"


static bool  IsLeapYear(int year)
{
    return  (year % 400 == 0 ||
            (year % 4 == 0 && year % 100 != 0));
}

static int DaysOfMonth(int year, int month)
{
    const int monthDay[13] = {-1, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if (2 == month && IsLeapYear(year))
        return 29;

    return  monthDay[month];
}



Time::Time() : m_ms(0), m_valid(false)
{
    this->Now();
}

Time::Time(const Time& other) : m_ms(other.m_ms)
{
    m_valid = false;
}


Time::Time(int hour, int min, int sec)
{
    this->Now();

    //如果hour小于当前的hour，则换天了，如果当前天数是本月最后一天，则换月；如果本月是12月，则换年
    int day   = GetDay();
    int mon   = GetMonth();
    int year  = GetYear();

    bool  tomorrow = false;
    if (hour < GetHour() ||
       (hour == GetHour() && min < GetMinute()) ||
       (hour == GetHour() && min == GetMinute() && sec < GetSecond()))
        tomorrow = true;

    if (tomorrow)
    {
        if (DaysOfMonth(year, mon) == day)
        {
            day = 1;

            if (12 == mon)
            {
                mon = 1;
                ++ year;
            }
            else
                ++ mon;
        }
        else
        {
            ++ day;
        }
    }

    // 构造tm
    struct tm  stm;
    stm.tm_sec = sec;
    stm.tm_min = min;
    stm.tm_hour= hour;
    stm.tm_mday= day;
    stm.tm_mon = mon - 1;
    stm.tm_year = year - 1900;
    stm.tm_yday = 0;
    stm.tm_isdst = 0;

    time_t tt = mktime(&stm);
    m_ms = tt * 1000UL;
    m_valid = false;
}

void Time::_UpdateTm()  const
{
    if (m_valid)  
        return;

    m_valid = true; 
    const time_t now(m_ms / 1000UL); 
    ::localtime_r(&now, &m_tm);
}

void Time::Now()
{
    struct timeval now;
    ::gettimeofday(&now, 0);
    m_ms = (uint64_t)(now.tv_sec * 1000UL + now.tv_usec / 1000UL);
    m_valid = false;
}


const char* Time::FormatTime(char* buf, int maxSize) const
{
    if (buf && maxSize > 1)
    {
        _UpdateTm();
        snprintf(buf, static_cast<size_t>(maxSize - 1), "%04d-%02d-%02d[%02d:%02d:%05d]", 
                m_tm.tm_year+1900, m_tm.tm_mon+1, m_tm.tm_mday,
                m_tm.tm_hour, m_tm.tm_min, m_tm.tm_sec * 1000 + m_ms % 1000UL);

        return buf;
    }
        
    return NULL;
}

void Time::AddDelay(uint64_t delay)
{
    m_ms   += delay;
    m_valid = false;
}

Time& Time::operator= (const Time & other)
{
    if (this != &other)
    {
        m_ms    = other.m_ms;
        m_valid = false;
    }
    return *this;
}

