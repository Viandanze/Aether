#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include "base/noncopyable.h"

class EventLoop;
class EventLoopThread;

// EventLoopThreadPool：主从Reactor的子Reactor线程池
// mainReactor(Acceptor)接收新连接 → round-robin分发给subReactor(IO线程)
// setThreadNum(0) = 单Reactor模式（主线程处理所有IO）
// setThreadNum(N) = 1主N从模式
class EventLoopThreadPool : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& name = std::string());
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    int  threadNum() const { return numThreads_; }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // Round-Robin获取下一个EventLoop（线程安全）
    // 如果线程池未启动，返回baseLoop（单Reactor模式）
    EventLoop* getNextLoop();

    // 获取所有EventLoop（用于定时器等需要遍历的场景）
    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string& name() const { return name_; }

private:
    EventLoop* baseLoop_;  // 主Reactor的EventLoop（TcpServer所在线程）
    std::string name_;
    bool started_;
    int numThreads_;

    std::atomic_int next_;  // Round-Robin索引

    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;  // Raw pointers to all sub-EventLoops
};
