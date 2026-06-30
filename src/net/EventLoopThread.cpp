#include "EventLoopThread.h"
#include "EventLoop.h"
#include "base/Logger.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,
                                 const std::string& name)
    : loop_(nullptr),
      exiting_(false),
      name_(name),
      callback_(cb) {}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

EventLoop* EventLoopThread::startLoop() {
    // Start thread, create EventLoop in thread function
    thread_ = std::thread([this]() { threadFunc(); });

    // Wait for EventLoop creation
    {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this]() { return loop_ != nullptr; });
    }

    LOG_INFO("EventLoopThread [%s] started, loop=%p", name_.c_str(), loop_);
    return loop_;
}

void EventLoopThread::threadFunc() {
    // Create EventLoop in IO thread (one loop per thread)
    EventLoop loop;

    if (callback_) {
        callback_(&loop);
    }

    {
        std::lock_guard<std::mutex> lock(mtx_);
        loop_ = &loop;
        cond_.notify_one();  // Notify main thread that EventLoop is created
    }

    loop.loop();  // Enter event loop (blocking)

    // After loop exits
    loop_ = nullptr;
}
