#include "EventLoopThreadPool.h"
#include "EventLoopThread.h"
#include "EventLoop.h"
#include "base/Logger.h"

EventLoopThreadPool::EventLoopThreadPool(EventLoop* baseLoop, const std::string& name)
    : baseLoop_(baseLoop),
      name_(name),
      started_(false),
      numThreads_(0),
      next_(0) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    // threads_ 中的 unique_ptr 会自动析构，EventLoopThread 析构时 quit + join
}

void EventLoopThreadPool::start(const ThreadInitCallback& cb) {
    started_ = true;

    for (int i = 0; i < numThreads_; ++i) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s-IO-%d", name_.c_str(), i);
        auto t = std::make_unique<EventLoopThread>(cb, std::string(buf));
        EventLoop* loop = t->startLoop();
        loops_.push_back(loop);
        threads_.push_back(std::move(t));
    }

    if (numThreads_ == 0 && cb) {
        // Single Reactor mode, call init callback directly on baseLoop
        cb(baseLoop_);
    }

    LOG_INFO("EventLoopThreadPool [%s] started with %d IO threads",
             name_.c_str(), numThreads_);
}

EventLoop* EventLoopThreadPool::getNextLoop() {
    // 单Reactor模式，所有连接由主线程处理
    if (loops_.empty()) {
        return baseLoop_;
    }

    // Round-Robin：原子递增取模，保证多线程安全
    int idx = next_.fetch_add(1) % loops_.size();
    return loops_[idx];
}

std::vector<EventLoop*> EventLoopThreadPool::getAllLoops() {
    if (loops_.empty()) {
        return std::vector<EventLoop*>(1, baseLoop_);
    }
    return loops_;
}
