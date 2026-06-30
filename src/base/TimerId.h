#pragma once
#include "Timer.h"

// TimerId: timer identifier for cancellation
// Contains Timer pointer and sequence number for correct cancellation
class TimerId {
public:
    TimerId() : timer_(nullptr), sequence_(0) {}
    TimerId(Timer* timer, int64_t seq) : timer_(timer), sequence_(seq) {}

    // Friend: TimerQueue needs access to private members
    friend class TimerQueue;

private:
    Timer* timer_;
    int64_t sequence_;
};
