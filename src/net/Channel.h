#pragma once
#include <functional>
#include <memory>

class EventLoop;

// Channel：fd的事件分发器
// 每个fd（socket）对应一个Channel，负责注册epoll事件并回调
class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void()>;

    // channel在epoller中的状态
    enum { kNew = -1, kAdded = 1, kDeleted = 2 };

    Channel(EventLoop* loop, int fd);
    ~Channel();

    void handleEvent();  // 核心方法：根据revents回调

    // 设置回调
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb)    { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb)    { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb)    { errorCallback_ = std::move(cb); }

    // Set interested events
    void enableReading()  { events_ |= kReadEvent; update(); }
    void enableWriting()  { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll()     { events_ = kNoneEvent; update(); }

    // ET模式
    void enableET() { events_ |= kET; update(); }

    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting()   const { return events_ & kWriteEvent; }

    int  fd()      const { return fd_; }
    int  events()  const { return events_; }
    void set_revents(int revt) { revents_ = revt; }
    int  index()   const { return index_; }
    void set_index(int idx) { index_ = idx; }

    EventLoop* ownerLoop() { return loop_; }
    void remove();  // 从EventLoop中移除自己

private:
    void update();

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;
    static const int kET;

    EventLoop* loop_;
    const int fd_;
    int events_;     // 注册的事件
    int revents_;    // epoll返回的就绪事件
    int index_;      // 在epoller中的状态

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
