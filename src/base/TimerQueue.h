#pragma once
#include <set>
#include <vector>
#include <memory>
#include "base/TimeStamp.h"
#include "base/Timer.h"
#include "base/TimerId.h"
#include "base/noncopyable.h"

class EventLoop;
class Channel;

// TimerQueue: timer queue based on timerfd + epoll
// 
// Design points:
// 1. Use timerfd to integrate timer events into epoll event loop
// 2. Use std::set to manage timers, sorted by expiration time
// 3. Only set timerfd to the earliest expiration time
// 4. Support one-shot and repeating timers
class TimerQueue : noncopyable {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // 在指定时间执行回调（一次性）
    TimerId addTimer(Timer::TimerCallback cb, TimeStamp when, double interval = 0.0);

    // 取消定时器
    void cancel(TimerId timerId);

private:
    // 用pair<TimeStamp, Timer*>作为set的key，保证唯一性
    using Entry = std::pair<TimeStamp, Timer*>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;  // Timer* + sequence
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);

    // timerfd可读回调
    void handleRead();

    // 获取所有已过期的定时器
    std::vector<Entry> getExpired(TimeStamp now);

    // 重置重复定时器，并更新timerfd
    void reset(const std::vector<Entry>& expired, TimeStamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerFd_;
    std::unique_ptr<Channel> timerFdChannel_;

    // Timer list sorted by expiration
    TimerList timers_;

    // Active timer set for cancel
    ActiveTimerSet activeTimers_;
    bool callingExpiredTimers_;
    ActiveTimerSet cancelingTimers_;  // 正在执行中的待取消列表
};
