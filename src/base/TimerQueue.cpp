#include "TimerQueue.h"
#include "net/EventLoop.h"
#include "net/Channel.h"
#include "base/Logger.h"
#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

// 创建timerfd
static int createTimerFd() {
    int fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0) {
        LOG_FATAL("timerfd_create failed: %s", strerror(errno));
    }
    return fd;
}

// 设置timerfd的过期时间
static void resetTimerFd(int fd, TimeStamp expiration) {
    struct itimerspec newValue;
    struct itimerspec oldValue;
    memset(&newValue, 0, sizeof(newValue));
    memset(&oldValue, 0, sizeof(oldValue));

    // When to trigger first
    newValue.it_value = expiration.toTimeSpec();
    // 不设间隔（由TimerQueue自己管理重复定时器）

    int ret = ::timerfd_settime(fd, TFD_TIMER_ABSTIME, &newValue, &oldValue);
    if (ret < 0) {
        LOG_ERROR("timerfd_settime failed: %s", strerror(errno));
    }
}

// 读取timerfd（必须读，否则epoll会持续触发）
static void readTimerFd(int fd) {
    uint64_t howmany;
    ssize_t n = ::read(fd, &howmany, sizeof(howmany));
    if (n != sizeof(howmany)) {
        LOG_ERROR("TimerQueue::handleRead reads %zd bytes instead of 8", n);
    }
}

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerFd_(createTimerFd()),
      timerFdChannel_(new Channel(loop, timerFd_)),
      timers_(),
      activeTimers_(),
      callingExpiredTimers_(false) {
    timerFdChannel_->setReadCallback([this]() { handleRead(); });
    timerFdChannel_->enableReading();
}

TimerQueue::~TimerQueue() {
    timerFdChannel_->disableAll();
    timerFdChannel_->remove();
    ::close(timerFd_);

    // 删除所有定时器
    for (auto& entry : timers_) {
        delete entry.second;
    }
}

TimerId TimerQueue::addTimer(Timer::TimerCallback cb, TimeStamp when, double interval) {
    auto* timer = new Timer(std::move(cb), when, interval);
    loop_->runInLoop([this, timer]() { addTimerInLoop(timer); });
    return TimerId(timer, timer->sequence());
}

void TimerQueue::addTimerInLoop(Timer* timer) {
    bool earliestChanged = insert(timer);

    if (earliestChanged) {
        // 最早的定时器变了，重置timerfd
        resetTimerFd(timerFd_, timer->expiration());
    }
}

void TimerQueue::cancel(TimerId timerId) {
    loop_->runInLoop([this, timerId]() { cancelInLoop(timerId); });
}

void TimerQueue::cancelInLoop(TimerId timerId) {
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    auto it = activeTimers_.find(timer);

    if (it != activeTimers_.end()) {
        size_t n = timers_.erase(Entry(it->first->expiration(), it->first));
        delete it->first;
        activeTimers_.erase(it);
    } else if (callingExpiredTimers_) {
        // 正在执行回调中的定时器，加入待取消列表
        cancelingTimers_.insert(timer);
    }
}

void TimerQueue::handleRead() {
    readTimerFd(timerFd_);

    TimeStamp now = TimeStamp::now();
    std::vector<Entry> expired = getExpired(now);

    callingExpiredTimers_ = true;
    cancelingTimers_.clear();

    for (auto& entry : expired) {
        entry.second->run();  // 执行定时器回调
    }

    callingExpiredTimers_ = false;
    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(TimeStamp now) {
    std::vector<Entry> expired;

    // sentinel：比now大的第一个
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    auto end = timers_.lower_bound(sentry);

    // [begin, end) 都是已过期的
    expired.assign(timers_.begin(), end);
    timers_.erase(timers_.begin(), end);

    // Synchronously remove from activeTimers_
    for (auto& entry : expired) {
        ActiveTimer timer(entry.second, entry.second->sequence());
        size_t n = activeTimers_.erase(timer);
        (void)n;
    }

    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, TimeStamp now) {
    TimeStamp nextExpire;

    for (auto& entry : expired) {
        ActiveTimer timer(entry.second, entry.second->sequence());

        // 如果在执行回调期间没有被取消，且是重复定时器，则重启
        if (entry.second->repeat() &&
            cancelingTimers_.find(timer) == cancelingTimers_.end()) {
            entry.second->restart(now);
            insert(entry.second);
        } else {
            // 不重复或已取消，删除Timer对象
            delete entry.second;
        }
    }

    // 设置下一个timerfd
    if (!timers_.empty()) {
        nextExpire = timers_.begin()->second->expiration();
        if (nextExpire.secondsFromNow() > 0) {
            resetTimerFd(timerFd_, nextExpire);
        }
    }
}

bool TimerQueue::insert(Timer* timer) {
    bool earliestChanged = false;
    TimeStamp when = timer->expiration();
    auto it = timers_.begin();

    if (it == timers_.end() || when < it->first) {
        earliestChanged = true;
    }

    timers_.insert(Entry(when, timer));
    activeTimers_.insert(ActiveTimer(timer, timer->sequence()));

    return earliestChanged;
}
