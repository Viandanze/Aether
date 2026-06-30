#pragma once
#include <functional>
#include <memory>
#include "base/noncopyable.h"

class EventLoop;
class InetAddress;
class Socket;
class Channel;

/// Acceptor：监听端口，接受新连接
class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(int fd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnectionCallback_ = std::move(cb);
    }

    void listen();
    bool listening() const { return listening_; }
    void stopListening();  // 停止监听（优雅关闭用）

private:
    void handleRead();

    EventLoop* loop_;
    std::unique_ptr<Socket> acceptSocket_;
    std::unique_ptr<Channel> acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
    int idleFd_;  // Idle fd for handling EMFILE (file descriptor exhaustion)
};
