#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>
#include <memory>
#include "base/noncopyable.h"
#include "base/TimeStamp.h"
#include "base/TimerId.h"

class Epoller;
class Channel;
class TimerQueue;
class TimerWheel;

/// EventLoop：事件循环，一个线程一个EventLoop
///
/// 核心职责：poll等待事件 → 分发到Channel处理 → 执行定时器 → 处理跨线程任务
/// 集成TimerWheel用于空闲连接超时管理
class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();
    void quit();

    // Channel管理
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);

    // 跨线程任务投递
    void runInLoop(Functor cb);
    void queueInLoop(Functor cb);
    void wakeup();

    bool isInLoopThread() const;

    void assertInLoopThread() {
        if (!isInLoopThread()) {
            LOG_FATAL("EventLoop::assertInLoopThread() failed");
        }
    }

    // ─── 定时器接口 ───
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
    const pid_t threadId_;

    std::unique_ptr<Epoller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    std::unique_ptr<TimerWheel> timerWheel_;

    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    std::mutex mtx_;
    std::vector<Functor> pendingFunctors_;
    std::atomic_bool callingPendingFunctors_;
};
