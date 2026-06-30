#pragma once
#include <functional>
#include <atomic>
#include "TimeStamp.h"

// Timer：定时器抽象
// 持有回调、过期时间、是否重复、间隔
// 由TimerQueue管理生命周期
class Timer {
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, TimeStamp when, double interval)
        : callback_(std::move(cb)),
          expiration_(when),
          interval_(interval),
          repeat_(interval > 0.0),
          sequence_(s_numCreated_++) {}

    void run() const { callback_(); }

    TimeStamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    // Restart repeating timer
    void restart(TimeStamp now) {
        if (repeat_) {
            expiration_ = TimeStamp::after(interval_ > 0 ? interval_ : 0.001);
        } else {
            expiration_ = TimeStamp();  // 不重复，置空
        }
    }

    static int64_t numCreated() { return s_numCreated_.load(); }

private:
    TimerCallback callback_;
    TimeStamp expiration_;
    double interval_;
    bool repeat_;
    int64_t sequence_;

    static std::atomic<int64_t> s_numCreated_;
};
