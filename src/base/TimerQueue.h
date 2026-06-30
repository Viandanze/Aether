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
// 2. Use std::set to manage timers, sorted by expiration
// 3. Only set timerfd to earliest expiration
// 4. Support one-shot and repeating timers
class TimerQueue : noncopyable {
public:
    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // Schedule callback at specified time (one-shot)
    TimerId addTimer(Timer::TimerCallback cb, TimeStamp when, double interval = 0.0);

    // Cancel timer
    void cancel(TimerId timerId);

private:
    // Use pair<TimeStamp, Timer*> as set key for uniqueness
    using Entry = std::pair<TimeStamp, Timer*>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;  // Timer* + sequence
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);

    // timerfd readable callback
    void handleRead();

    // Get all expired timers
    std::vector<Entry> getExpired(TimeStamp now);

    // Reset repeating timers and update timerfd
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
    ActiveTimerSet cancelingTimers_;  // Pending cancel list for timers being executed
};
