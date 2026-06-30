#include "TimerWheel.h"
#include "net/EventLoop.h"
#include "net/TcpConnection.h"
#include "base/Logger.h"

TimerWheel::TimerWheel(EventLoop* loop, int timeoutSeconds)
    : timeoutSeconds_(timeoutSeconds),
      capacity_(static_cast<size_t>(timeoutSeconds) + 1),
      current_(0),
      loop_(loop),
      wheel_(capacity_) {
}

TimerWheel::~TimerWheel() {
    stop();
}

void TimerWheel::start() {
    // Tick once per second
    tickTimerId_ = loop_->runEvery(1.0, [this]() { tick(); });
    LOG_INFO("TimerWheel started: timeout=%ds, slots=%zu", timeoutSeconds_, capacity_);
}

void TimerWheel::stop() {
    loop_->cancel(tickTimerId_);
}

void TimerWheel::insert(const std::shared_ptr<TcpConnection>& conn) {
    // 递增generation使旧条目失效
    uint64_t gen = ++conn->idleTimerGeneration();
    size_t slot = (current_ + static_cast<size_t>(timeoutSeconds_)) % capacity_;
    wheel_[slot].push_back({conn, gen});
}

void TimerWheel::tick() {
    current_ = (current_ + 1) % capacity_;
    auto& slot = wheel_[current_];

    for (auto& entry : slot) {
        auto conn = entry.conn.lock();
        if (conn && conn->idleTimerGeneration() == entry.generation) {
            // generation匹配→连接自上次insert以来没有新活动→空闲超时
            if (conn->connected()) {
                LOG_INFO("TimerWheel: idle timeout, closing [%s]", conn->name().c_str());
                conn->shutdown();
            }
        }
        // else: 过期条目（连接已刷新或已销毁），忽略
    }

    slot.clear();
}
