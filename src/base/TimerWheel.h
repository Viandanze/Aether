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
/// - Single-level wheel, tick per second, N slots (N = timeout seconds + 1)
/// - Each slot holds (weak_ptr<TcpConnection>, generation) pairs
/// - On connection activity (insert): place in slot (current + timeout) % capacity
/// - On tick: check current slot - generation match -> idle timeout -> shutdown; no match -> expired -> ignore
/// - To refresh: just ++generation and reinsert, old entries auto-invalidated
///
/// vs old approach (one runAfter Timer per connection):
/// - Old: N connections = N Timer objects + N timerfd_settime calls
/// - New: N connections = 1 one-second timer + O(k) tick (k = entries in current slot)
class TimerWheel : noncopyable {
public:
    TimerWheel(EventLoop* loop, int timeoutSeconds);
    ~TimerWheel();

    void start();
    void stop();

    /// Insert or refresh a connection's idle timeout
    /// Must be called in EventLoop thread
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
