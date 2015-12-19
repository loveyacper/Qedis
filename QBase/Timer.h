
#ifndef BERT_TIMER_H
#define BERT_TIMER_H

#include <vector>
#include <ctime>
#include <sys/time.h>
#include <stdint.h>
#include <memory>
#include <mutex>
#include <atomic>

uint64_t Now();

class Time
{
public:
    Time();
    Time(const Time& other);
    Time(int hour, int min, int sec); // TODO year month day, and Effective cpp item 18

    void        Now();
    uint64_t    MilliSeconds() const { return ms_; }
    std::size_t FormatTime(char* buf) const;
    void        AddDelay(uint64_t delay);

    int GetYear()   const { _UpdateTm(); return tm_.tm_year + 1900;  } 
    int GetMonth()  const { _UpdateTm(); return tm_.tm_mon + 1;  } 
    int GetDay()    const { _UpdateTm(); return tm_.tm_mday; }
    int GetHour()   const { _UpdateTm(); return tm_.tm_hour; }
    int GetMinute() const { _UpdateTm(); return tm_.tm_min;  }
    int GetSecond() const { _UpdateTm(); return tm_.tm_sec;  }

    Time&    operator= (const Time& );
    operator uint64_t() const { return ms_; }

private:
    uint64_t    ms_;      // milliseconds from 1970
    mutable tm  tm_;
    mutable bool valid_;
    
    void    _UpdateTm()  const;
};


class Timer;
typedef std::shared_ptr<Timer>  PTIMER;

class Timer
{
    friend class TimerManager;
public:
    explicit Timer(uint32_t interval = uint32_t(-1), int32_t count = -1);
    virtual ~Timer() {}
    bool     OnTimer();
    void     SetRemainCnt(int32_t remain) {  count_ = remain; }
    bool     IsWorking() const {  return  prev_.get() != nullptr; }

private:
    virtual  bool _OnTimer() { return false; }
    PTIMER   next_;
    PTIMER   prev_;
    Time     triggerTime_;
    uint32_t interval_;
    int32_t  count_;
};

class TimerManager
{
public:
    ~TimerManager();

    bool    UpdateTimers(const Time& now);
    void    ScheduleAt(const PTIMER& pTimer, const Time& triggerTime);
    void    AddTimer(const PTIMER& pTimer);
    void    AsyncAddTimer(const PTIMER& pTimer);
    void    KillTimer(const PTIMER& pTimer);

    static  TimerManager&   Instance();

private:
    TimerManager();

    bool    _Cacsade(PTIMER pList[], int index);
    int     _Index(int level);

    static const int LIST1_BITS = 8;
    static const int LIST_BITS  = 6;
    static const int LIST1_SIZE = 1 << LIST1_BITS;
    static const int LIST_SIZE  = 1 << LIST_BITS;

    Time  m_lastCheckTime;

    PTIMER m_list1[LIST1_SIZE]; // 256 ms
    PTIMER m_list2[LIST_SIZE];  // 64 * 256ms = 16s
    PTIMER m_list3[LIST_SIZE];  // 64 * 64 * 256ms = 17m
    PTIMER m_list4[LIST_SIZE];  // 64 * 64 * 64 * 256ms = 18h
    PTIMER m_list5[LIST_SIZE];  // 64 * 64 * 64 * 64 * 256ms = 49d

    // async add
    std::mutex               lock_;
    std::atomic<std::size_t> count_;
    std::vector<PTIMER >  timers_;
};

inline int TimerManager::_Index(int level)
{
    uint64_t current = m_lastCheckTime;
    current >>= (LIST1_BITS + level * LIST_BITS);
    return  current & (LIST_SIZE - 1);
}

#endif

