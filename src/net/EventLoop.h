#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include <thread>
#include "base/noncopyable.h"
#include "base/TimeStamp.h"
#include "base/TimerId.h"

class Epoller;
class Channel;
class TimerQueue;
class TimerWheel;

/// EventLoop: one event loop per thread
///
/// Core duties: poll -> dispatch to Channel -> run timers -> handle cross-thread tasks
/// Integrates TimerWheel for idle connection timeout management
class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    // Channel management
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    // Cross-thread task dispatch
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);
    void wakeup();

    bool isInLoopThread() const;

    void assertInLoopThread() {
        if (!isInLoopThread()) {
            LOG_FATAL("EventLoop::assertInLoopThread() failed");
        }
    }

    // ─── Timer interface ───
    TimerId runAt(TimeStamp time, Timer::TimerCallback cb);
    TimerId runAfter(double delay, Timer::TimerCallback cb);
    TimerId runEvery(double interval, Timer::TimerCallback cb);
    void cancel(TimerId timerId);

    // ─── TimerWheel (idle timeout) ───
    void setIdleTimeout(int seconds);
    void insertToWheel(const std::shared_ptr<class TcpConnection>& conn);

private:
    void handleRead();
    void doPendingFunctors();

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;
    std::atomic_bool quit_;
    const std::thread::id threadId_;

    std::unique_ptr<Epoller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    std::unique_ptr<TimerWheel> timerWheel_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    std::mutex mtx_;
    std::vector<Functor> pendingFunctors_;
    std::atomic_bool callingPendingFunctors_;
};
