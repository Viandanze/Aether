#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include "base/noncopyable.h"
#include "base/TimerId.h"

class EventLoop;
class TcpConnection;

/// TimerWheel: O(1) idle connection timeout management
///
/// Design:
/// - Single-level timing wheel, tick once per second, N slots (N = timeout seconds + 1)
/// - Each slot holds (weak_ptr<TcpConnection>, generation) pairs
/// - When connection is active: insert into (current + timeout) % capacity slot
/// - On tick: check current slot - generation match → idle timeout → shutdown; no match → expired → ignore
/// - To refresh connection: just ++generation and reinsert, old entries automatically invalidated
///
/// Comparison with old approach (one runAfter Timer per connection):
/// - Old: N connections = N Timer objects + N timerfd_settime calls
/// - New: N connections = 1 one-second timer + O(k) tick (k = entries in current slot)
class TimerWheel : noncopyable {
public:
    TimerWheel(EventLoop* loop, int timeoutSeconds);
    ~TimerWheel();

    void start();
    void stop();

    /// 插入或刷新一个连接的空闲计时
    /// 必须在EventLoop线程中调用
    void insert(const std::shared_ptr<TcpConnection>& conn);

private:
    void tick();

    struct SlotEntry {
        std::weak_ptr<TcpConnection> conn;
        uint64_t generation;  // Used to determine if entry is expired
    };

    int timeoutSeconds_;
    size_t capacity_;
    size_t current_;
    EventLoop* loop_;
    TimerId tickTimerId_;
    std::vector<std::vector<SlotEntry>> wheel_;
};
