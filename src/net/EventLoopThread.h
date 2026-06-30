#pragma once
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "base/noncopyable.h"

class EventLoop;

// EventLoopThread：封装一个IO线程 + 其拥有的EventLoop
// 启动线程后等待EventLoop创建完成再返回，确保线程安全
class EventLoopThread : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback(),
                    const std::string& name = std::string());
    ~EventLoopThread();

    EventLoop* startLoop();  // 启动线程并返回其EventLoop

private:
    void threadFunc();  // 线程入口函数

    EventLoop* loop_;  // EventLoop created by thread (raw pointer, lifecycle managed by thread)
    bool exiting_;
    std::string name_;
    std::thread thread_;
    std::mutex mtx_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;  // 线程初始化回调
};
