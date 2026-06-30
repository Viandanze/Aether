#pragma once
#include "Timer.h"

// TimerId: timer identifier for cancellation
// Contains Timer pointer and sequence number to ensure correct timer cancellation
class TimerId {
public:
    TimerId() : timer_(nullptr), sequence_(0) {}
    TimerId(Timer* timer, int64_t seq) : timer_(timer), sequence_(seq) {}

    // 友元：TimerQueue需要访问私有成员
    friend class TimerQueue;

private:
    Timer* timer_;
    int64_t sequence_;
};
