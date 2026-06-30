#pragma once
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "base/noncopyable.h"

class EventLoop;

// EventLoopThread: wraps an IO thread + its owned EventLoop
// Waits for EventLoop creation before returning, ensuring thread safety
class EventLoopThread : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                    const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();  // Start thread and return its EventLoop

private:
    void threadFunc();  // Thread entry function

    EventLoop* loop_;  // EventLoop created by thread (raw pointer, lifecycle managed by thread)
    bool exiting_;
    std::string name_;
    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;  // Thread initialization callback
};
