#pragma once
#include <chrono>
#include <string>
#include <cstdio>

// TimeStamp：时间戳封装，基于steady_clock用于定时器
// 同时提供system_clock用于日志时间戳
class TimeStamp {
public:
    // 微秒精度
    using MicroSeconds = std::chrono::microseconds;
    using SteadyTimePoint = std::chrono::steady_clock::time_point;
    using SystemTimePoint = std::chrono::system_clock::time_point;

    TimeStamp() : steadyTime_() {}

    explicit TimeStamp(SteadyTimePoint t) : steadyTime_(t) {}

    // 从现在起delay秒后的时间点
    static TimeStamp now() {
        return TimeStamp(std::chrono::steady_clock::now());
    }

    static TimeStamp after(double seconds) {
        return TimeStamp(std::chrono::steady_clock::now() +
                         std::chrono::duration_cast<MicroSeconds>(
                             std::chrono::duration<double>(seconds)));
    }

    // Time difference from now (seconds)
    double secondsFromNow() const {
        auto diff = steadyTime_ - std::chrono::steady_clock::now();
        return std::chrono::duration<double>(diff).count();
    }

    // 距离另一个时间点的时间差（秒）
    double diffSeconds(const TimeStamp& other) const {
        auto diff = steadyTime_ - other.steadyTime_;
        return std::chrono::duration<double>(diff).count();
    }

    bool operator<(const TimeStamp& rhs) const { return steadyTime_ < rhs.steadyTime_; }
    bool operator<=(const TimeStamp& rhs) const { return steadyTime_ <= rhs.steadyTime_; }
    bool operator>(const TimeStamp& rhs) const { return steadyTime_ > rhs.steadyTime_; }
    bool operator==(const TimeStamp& rhs) const { return steadyTime_ == rhs.steadyTime_; }

    SteadyTimePoint steadyTime() const { return steadyTime_; }

    // 转换为timespec（用于timerfd_settime）
    struct timespec toTimeSpec() const {
        auto dur = steadyTime_.time_since_epoch();
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(dur);
        auto nsecs = std::chrono::duration_cast<std::chrono::nanoseconds>(dur - secs);
        return {secs.count(), nsecs.count()};
    }

    // 可读时间字符串（用于日志）
    static std::string nowString() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t_now));
        return buf;
    }

private:
    SteadyTimePoint steadyTime_;
};
