
#ifndef BERT_TIMER_H
#define BERT_TIMER_H

#include <vector>
#include <set>
#include <ctime>
#include <sys/time.h>
#include <stdint.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <functional>

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


class Timer
{
    friend class TimerManager;
public:
    void  Init(uint32_t interval, int32_t count = -1);
    bool  OnTimer();
    void  SetRemainCnt(int32_t remain) {  count_ = remain; }
    bool  IsWorking() const {  return  prev_ != nullptr; }

    template <typename F, typename... Args>
    void SetCallback(F&& f, Args&&... args)
    {
        auto temp = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        func_ = [temp]() { (void)temp(); };
    }

private:
    Timer();

    std::function<void ()> func_;
    Timer* next_;
    Timer* prev_;
    Time     triggerTime_;
    uint32_t interval_;
    int32_t  count_;
};

class TimerManager
{
public:
    ~TimerManager();

    TimerManager(const TimerManager& ) = delete;
    void operator= (const TimerManager& ) = delete;

    static  TimerManager&   Instance();

    bool    UpdateTimers(const Time& now);
    void    ScheduleAt(Timer* pTimer, const Time& triggerTime);
    void    AddTimer(Timer* timer);
    void    AsyncAddTimer(Timer* timer);
    void    KillTimer(Timer* pTimer);

    Timer*  CreateTimer();

private:
    TimerManager();

    bool _Cacsade(Timer* pList[], int index);
    int _Index(int level);

    static const int LIST1_BITS = 8;
    static const int LIST_BITS  = 6;
    static const int LIST1_SIZE = 1 << LIST1_BITS;
    static const int LIST_SIZE  = 1 << LIST_BITS;

    Time  m_lastCheckTime;

    Timer* m_list1[LIST1_SIZE]; // 256 ms
    Timer* m_list2[LIST_SIZE];  // 64 * 256ms = 16s
    Timer* m_list3[LIST_SIZE];  // 64 * 64 * 256ms = 17m
    Timer* m_list4[LIST_SIZE];  // 64 * 64 * 64 * 256ms = 18h
    Timer* m_list5[LIST_SIZE];  // 64 * 64 * 64 * 64 * 256ms = 49 days

    // timer pool
    std::set<Timer* > freepool_;

    // async add
    std::mutex lock_;
    std::atomic<std::size_t> count_;
    std::vector<Timer* > timers_;
};

inline int TimerManager::_Index(int level)
{
    uint64_t current = m_lastCheckTime;
    current >>= (LIST1_BITS + level * LIST_BITS);
    return current & (LIST_SIZE - 1);
}

#endif

