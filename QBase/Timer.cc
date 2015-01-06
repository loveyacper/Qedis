#include <cstdlib>
#include <cstdio>
#include <cassert>
#include "Timer.h"
#include "Threads/Atomic.h"

uint64_t Now()
{
    struct timeval now;
    ::gettimeofday(&now, 0);
    return  uint64_t(now.tv_sec * 1000UL + now.tv_usec / 1000UL);
}

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


Time  g_now;

Time::Time() : m_ms(0), m_valid(false)
{
    m_tm.tm_year = 0;
    this->Now();
}

Time::Time(const Time& other) : m_ms(other.m_ms)
{
    m_tm.tm_year = 0;
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
    m_ms = ::Now();
    m_valid = false;
}

// from 2015 to 2025
static const char* YEAR[] = { "2015", "2016", "2017", "2018",
    "2019", "2020", "2021", "2022", "2023", "2024", "2025",
     nullptr,
};

const char* Time::FormatTime(char* buf, int maxSize) const
{
    static char NUMBER[60][2] = {""};

    static bool bFirst = true;
    if (bFirst)
    {
        bFirst = false;
        for (size_t i = 0; i < sizeof NUMBER / sizeof NUMBER[0]; ++ i)
        {
            char tmp[3]; 
            snprintf(tmp, 3, "%02d", static_cast<int>(i));
            memcpy(NUMBER[i], tmp, 2);
        }
    }

    if (buf && maxSize > 1)
    {
        _UpdateTm();
#if  1
        memcpy(buf, YEAR[m_tm.tm_year + 1900 - 2015], 4);
        buf[4] = '-';
        memcpy(buf + 5, NUMBER[m_tm.tm_mon + 1], 2);
        buf[7] = '-';
        memcpy(buf + 8, NUMBER[m_tm.tm_mday], 2);
        buf[10] = '[';
        memcpy(buf + 11, NUMBER[m_tm.tm_hour], 2);
        buf[13] = ':';
        memcpy(buf + 14, NUMBER[m_tm.tm_min], 2);
        buf[16] = ':';
        memcpy(buf + 17, NUMBER[m_tm.tm_sec], 2);
        buf[19] = '.';
        snprintf(buf + 20, 4, "%03d", static_cast<int>(m_ms % 1000));
        memcpy(buf + 23, "]", 2);
#else
        snprintf(buf, static_cast<size_t>(maxSize), "%04d-%02d-%02d[%02d:%02d:%02d.%03d]", 
                m_tm.tm_year+1900, m_tm.tm_mon+1, m_tm.tm_mday,
                m_tm.tm_hour, m_tm.tm_min, m_tm.tm_sec,
                static_cast<int>(m_ms % 1000));
#endif

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




Timer::Timer(uint32_t interval, int32_t count) :
m_interval(interval),
m_count(count)
{
    m_triggerTime.AddDelay(interval);
}


bool Timer::OnTimer()
{
    if (m_count < 0 || -- m_count >= 0)
    {
        m_triggerTime.AddDelay(m_interval);
        return  _OnTimer();
    }

    return false;        
}

TimerManager::TimerManager() : m_count(0)
{
    for (int i = 0; i < LIST1_SIZE; ++ i)
    {
        m_list1[i].Reset(new Timer);
    }

    for (int i = 0; i < LIST_SIZE; ++ i)
    {
        m_list2[i].Reset(new Timer);
        m_list3[i].Reset(new Timer);
        m_list4[i].Reset(new Timer);
        m_list5[i].Reset(new Timer);
    }
}

TimerManager::~TimerManager()
{
    PTIMER   pTimer;
    for (int i = 0; i < LIST1_SIZE; ++ i)
    {
        while ((pTimer = m_list1[i]->m_next) != NULL)
        {
            KillTimer(pTimer);
        }
    }

    for (int i = 0; i < LIST_SIZE; ++ i)
    {
        while ((pTimer = m_list2[i]->m_next) != NULL)
        {
            KillTimer(pTimer);
        }

        while ((pTimer = m_list3[i]->m_next) != NULL)
        {
            KillTimer(pTimer);
        }

        while ((pTimer = m_list4[i]->m_next) != NULL)
        {
            KillTimer(pTimer);
        }

        while ((pTimer = m_list5[i]->m_next) != NULL)
        {
            KillTimer(pTimer);
        }
    }
}

TimerManager&  TimerManager::Instance()
{
    static TimerManager mgr;
    return  mgr;
}

bool TimerManager::UpdateTimers(const Time& now)
{
    if (AtomicGet(&m_count) > 0 && m_lock.TryLock())
    {
        std::vector<PTIMER >  tmp;
        tmp.swap(m_timers);
        m_count = 0;
        m_lock.Unlock();

        for (std::vector<PTIMER >::iterator it(tmp.begin());
             it != tmp.end();
             ++ it)
        {
            AddTimer(*it);
        }
    }

    const bool   hasUpdated(m_lastCheckTime <= now);

    while (m_lastCheckTime <= now)
    {
        int index = m_lastCheckTime & (LIST1_SIZE - 1);
        if (index == 0 &&
            !_Cacsade(m_list2, _Index(0)) &&
            !_Cacsade(m_list3, _Index(1)) &&
            !_Cacsade(m_list4, _Index(2)))
        {
            _Cacsade(m_list5, _Index(3));
        }

        m_lastCheckTime.AddDelay(1);

        PTIMER   pTimer;
        while ((pTimer = m_list1[index]->m_next) != NULL)
        {
            KillTimer(pTimer);
            if (pTimer->OnTimer())
                AddTimer(pTimer);
        }
    }        

    return hasUpdated;
}


void TimerManager::AddTimer(const PTIMER& pTimer)
{
    KillTimer(pTimer);

    uint32_t diff      =  static_cast<uint32_t>(pTimer->m_triggerTime - m_lastCheckTime);
    PTIMER   pListHead ;
    uint64_t trigTime  =  pTimer->m_triggerTime.MilliSeconds();

    if ((int32_t)diff < 0)
    {
        pListHead = m_list1[m_lastCheckTime.MilliSeconds() & (LIST1_SIZE - 1)];
    }
    else if (diff < static_cast<uint32_t>(LIST1_SIZE))
    {
        pListHead = m_list1[trigTime & (LIST1_SIZE - 1)];
    }
    else if (diff < 1 << (LIST1_BITS + LIST_BITS))
    {
        pListHead = m_list2[(trigTime >> LIST1_BITS) & (LIST_SIZE - 1)];
    }
    else if (diff < 1 << (LIST1_BITS + 2 * LIST_BITS))
    {
        pListHead = m_list3[(trigTime >> (LIST1_BITS + LIST_BITS)) & (LIST_SIZE - 1)];
    }
    else if (diff < 1 << (LIST1_BITS + 3 * LIST_BITS))
    {
        pListHead = m_list4[(trigTime >> (LIST1_BITS + 2 * LIST_BITS)) & (LIST_SIZE - 1)];
    }
    else
    {
        pListHead = m_list5[(trigTime >> (LIST1_BITS + 3 * LIST_BITS)) & (LIST_SIZE - 1)];
    }

    assert(!pListHead->m_prev);

    pTimer->m_prev = pListHead;
    pTimer->m_next = pListHead->m_next;
    if (pListHead->m_next)
        pListHead->m_next->m_prev = pTimer;
    pListHead->m_next = pTimer;
}

void    TimerManager::AsyncAddTimer(const PTIMER&  pTimer)
{
    ScopeMutex  guard(m_lock);
    m_timers.push_back(pTimer);
    AtomicChange(&m_count, 1);
    assert (m_count == static_cast<int>(m_timers.size()));
}

void    TimerManager::ScheduleAt(const PTIMER& pTimer, const Time& triggerTime)
{
    if (!pTimer)
        return;

    pTimer->m_triggerTime = triggerTime;
    AddTimer(pTimer);
}


void TimerManager::KillTimer(const PTIMER& pTimer)
{
    if (pTimer && pTimer->m_prev)
    {
        pTimer->m_prev->m_next = pTimer->m_next;

        if (pTimer->m_next)
        {
            pTimer->m_next->m_prev = pTimer->m_prev;
            pTimer->m_next.Reset();
        }

        if (pTimer->m_prev)
            pTimer->m_prev.Reset();
    }
}

bool TimerManager::_Cacsade(PTIMER pList[], int index)
{
    assert (pList);

    if (index < 0 ||
        index >= LIST_SIZE)
        return  false;

    assert (pList[index]);

    if (!pList[index]->m_next)
        return false;

    PTIMER  tmpListHead = pList[index]->m_next;
    pList[index]->m_next.Reset();

    while (tmpListHead != NULL)
    {
        PTIMER next = tmpListHead->m_next;
        if (tmpListHead->m_next)
            tmpListHead->m_next.Reset();
        if (tmpListHead->m_prev)
            tmpListHead->m_prev.Reset();
        AddTimer(tmpListHead);
        tmpListHead = next;
    }

    return true;
}

