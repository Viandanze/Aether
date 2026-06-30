#pragma once
#include <functional>
#include <memory>

class EventLoop;

// Channel: event dispatcher for fd
// One Channel per fd (socket), handles epoll event registration and callbacks
class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void()>;

    // Channel state in epoller
    enum { kNew = -1, kAdded = 1, kDeleted = 2 };

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent();  // Core method: dispatch based on revents

    // Set callbacks
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb)    { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb)    { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb)    { errorCallback_ = std::move(cb); }

    // Set interested events
    void enableReading()  { events_ |= kReadEvent; update(); }
    void enableWriting()  { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll()     { events_ = kNoneEvent; update(); }

    // ET mode
    void enableET() { events_ |= kET; update(); }

    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting()   const { return events_ & kWriteEvent; }

    int  fd()      const { return fd_; }
    int  events()  const { return events_; }
    void set_revents(int revt) { revents_ = revt; }
    int  index()   const { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop* ownerLoop() { return loop_; }
    void remove();  // Remove self from EventLoop

private:
    void update();

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;
    static const int kET;

    EventLoop* loop_;
    const int fd_;
    int events_;     // Registered events
    int revents_;    // Ready events returned by epoll
    int index_;      // State in epoller

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
