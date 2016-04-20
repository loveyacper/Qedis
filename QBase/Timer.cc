#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include "Timer.h"

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



Time::Time() : ms_(0), valid_(false)
{
    tm_.tm_year = 0;
    this->Now();
}

Time::Time(const Time& other) : ms_(other.ms_)
{
    tm_.tm_year = 0;
    valid_ = false;
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
    ms_ = tt * 1000UL;
    valid_ = false;
}

void Time::_UpdateTm()  const
{
    if (valid_)  
        return;

    valid_ = true; 
    const time_t now(ms_ / 1000UL); 
    ::localtime_r(&now, &tm_);
}

void Time::Now()
{
    ms_ = ::Now();
    valid_ = false;
}

// from 2015 to 2025
static const char* YEAR[] = { "2015", "2016", "2017", "2018",
    "2019", "2020", "2021", "2022", "2023", "2024", "2025",
     nullptr,
};

std::size_t Time::FormatTime(char* buf) const
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

    _UpdateTm();
    
#if  1
    memcpy(buf, YEAR[tm_.tm_year + 1900 - 2015], 4);
    buf[4] = '-';
    memcpy(buf + 5, NUMBER[tm_.tm_mon + 1], 2);
    buf[7] = '-';
    memcpy(buf + 8, NUMBER[tm_.tm_mday], 2);
    buf[10] = '[';
    memcpy(buf + 11, NUMBER[tm_.tm_hour], 2);
    buf[13] = ':';
    memcpy(buf + 14, NUMBER[tm_.tm_min], 2);
    buf[16] = ':';
    memcpy(buf + 17, NUMBER[tm_.tm_sec], 2);
    buf[19] = '.';
    snprintf(buf + 20, 5, "%03d]", static_cast<int>(ms_ % 1000));
#else
    
    snprintf(buf, 25, "%04d-%02d-%02d[%02d:%02d:%02d.%03d]",
             tm_.tm_year+1900, tm_.tm_mon+1, tm_.tm_mday,
             tm_.tm_hour, tm_.tm_min, tm_.tm_sec,
             static_cast<int>(ms_ % 1000));
#endif
    
    return 24;
}

void Time::AddDelay(uint64_t delay)
{
    ms_   += delay;
    valid_ = false;
}

Time& Time::operator= (const Time & other)
{
    if (this != &other)
    {
        ms_    = other.ms_;
        valid_ = false;
    }
    return *this;
}




Timer::Timer(): next_(nullptr), prev_(nullptr)
{
}

void  Timer::Init(uint32_t interval, int32_t count)
{
    interval_ = interval;
    count_ = count;
    triggerTime_.Now();
    triggerTime_.AddDelay(interval);
}


bool Timer::OnTimer()
{
    if (!func_)
        return false;

    if (count_ < 0 || -- count_ >= 0)
    {
        triggerTime_.AddDelay(interval_);
        func_();

        return true;
    }

    return false;        
}

TimerManager::TimerManager() : count_(0)
{
    for (int i = 0; i < LIST1_SIZE; ++ i)
    {
        m_list1[i] = new Timer();
    }

    for (int i = 0; i < LIST_SIZE; ++ i)
    {
        m_list2[i] = new Timer();
        m_list3[i] = new Timer();
        m_list4[i] = new Timer();
        m_list5[i] = new Timer();
    }
}

TimerManager::~TimerManager()
{
    Timer* pTimer = nullptr;
    for (int i = 0; i < LIST1_SIZE; ++ i)
    {
        while ((pTimer = m_list1[i]->next_) )
        {
            KillTimer(pTimer);
            delete pTimer;
        }
    
        delete m_list1[i];
    }

    for (int i = 0; i < LIST_SIZE; ++ i)
    {
        while ((pTimer = m_list2[i]->next_) )
        {
            KillTimer(pTimer);
            delete pTimer;
        }
        delete m_list2[i];

        while ((pTimer = m_list3[i]->next_) )
        {
            KillTimer(pTimer);
            delete pTimer;
        }
        delete m_list3[i];

        while ((pTimer = m_list4[i]->next_) )
        {
            KillTimer(pTimer);
            delete pTimer;
        }
        delete m_list4[i];

        while ((pTimer = m_list5[i]->next_) )
        {
            KillTimer(pTimer);
            delete pTimer;
        }
        delete m_list5[i];
    }
    
    for (auto t : freepool_)
        delete t;
}

TimerManager&  TimerManager::Instance()
{
    static TimerManager mgr;
    return  mgr;
}

Timer* TimerManager::CreateTimer()
{
    Timer* timer = nullptr;
    if (freepool_.empty())
    {
        timer = new Timer();
        freepool_.insert(timer);
    }
    else
    {
        timer = *(freepool_.begin());
        freepool_.erase(freepool_.begin());
    }

    return timer;
}

bool TimerManager::UpdateTimers(const Time& now)
{
    if (count_ > 0 && lock_.try_lock())
    {
        decltype(timers_) tmp;
        tmp.swap(timers_);
        count_ = 0;
        lock_.unlock();

        for (auto timer : tmp)
        {
            AddTimer(timer);
        }
    }

    const bool hasUpdated(m_lastCheckTime <= now);

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

        Timer* pTimer;
        while ((pTimer = m_list1[index]->next_))
        {
            KillTimer(pTimer);
            if (pTimer->OnTimer())
                AddTimer(pTimer);
            else
                freepool_.insert(pTimer);
        }
    }        

    return hasUpdated;
}


void TimerManager::AddTimer(Timer* timer)
{
    uint32_t diff      =  static_cast<uint32_t>(timer->triggerTime_ - m_lastCheckTime);
    uint64_t trigTime  =  timer->triggerTime_.MilliSeconds();
    Timer* pListHead   = nullptr;
    
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

    assert(!pListHead->prev_);

    timer->prev_ = pListHead;
    timer->next_ = pListHead->next_;
    if (pListHead->next_)
        pListHead->next_->prev_ = timer;
    pListHead->next_ = timer;
    
    freepool_.erase(timer);
}

void TimerManager::AsyncAddTimer(Timer* timer)
{
    std::lock_guard<std::mutex>  guard(lock_);

    timers_.push_back(timer);
    ++ count_;
    assert (count_ == timers_.size());
}

void TimerManager::ScheduleAt(Timer* pTimer, const Time& triggerTime)
{
    if (!pTimer)
        return;

    pTimer->triggerTime_ = triggerTime;
    AddTimer(pTimer);
}


void TimerManager::KillTimer(Timer* pTimer)
{
    if (!pTimer)
        return;

    if (pTimer->prev_)
    {
        pTimer->prev_->next_ = pTimer->next_;

        if (pTimer->next_)
        {
            pTimer->next_->prev_ = pTimer->prev_;
            pTimer->next_ = nullptr;
        }

        pTimer->prev_ = nullptr;
    }
}

bool TimerManager::_Cacsade(Timer* pList[], int index)
{
    assert (pList);

    if (index < 0 ||
        index >= LIST_SIZE)
        return  false;

    assert (pList[index]);

    if (!pList[index]->next_)
        return false;

    Timer* tmpListHead = pList[index]->next_;
    pList[index]->next_ = nullptr;

    while (tmpListHead)
    {
        Timer* next = tmpListHead->next_;

        tmpListHead->next_ = nullptr;
        tmpListHead->prev_ = nullptr;
        
        AddTimer(tmpListHead);
        tmpListHead = next;
    }

    return true;
}

