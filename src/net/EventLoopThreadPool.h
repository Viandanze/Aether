#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include "base/noncopyable.h"

class EventLoop;
class EventLoopThread;

// EventLoopThreadPool: sub-Reactor thread pool for master-slave Reactor
// mainReactor(Acceptor) accepts new connections -> round-robin to subReactor(IO threads)
// setThreadNum(0) = single Reactor mode (main thread handles all IO)
// setThreadNum(N) = 1 master + N slaves mode
class EventLoopThreadPool : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;

    EventLoopThreadPool(EventLoop* baseLoop, const std::string& name = std::string());
    ~EventLoopThreadPool();

    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
    int  threadNum() const { return numThreads_; }

    void start(const ThreadInitCallback& cb = ThreadInitCallback());

    // Round-Robin get next EventLoop (thread-safe)
    // If pool not started, return baseLoop (single Reactor mode)
    EventLoop* getNextLoop();

    // Get all EventLoops (for timers and other traversal needs)
    std::vector<EventLoop*> getAllLoops();

    bool started() const { return started_; }
    const std::string& name() const { return name_; }

private:
    EventLoop* baseLoop_;  // Main Reactor's EventLoop (thread where TcpServer lives)
    std::string name_;
    bool started_;
    int numThreads_;

    std::atomic_int next_;  // Round-Robin index

    std::vector<std::unique_ptr<EventLoopThread>> threads_;
    std::vector<EventLoop*> loops_;  // Raw pointers to all sub-EventLoops
};
