#pragma once
#include "base/noncopyable.h"

// RAII封装socket fd
class Socket : noncopyable {
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket();

    int fd() const { return fd_; }

    // 服务端socket操作
    void bindAddress(const class InetAddress& localAddr);
    void listen();
    int  accept(class InetAddress* peerAddr);

    // socket选项
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setTcpNoDelay(bool on);
    void setKeepAlive(bool on);

    // 关闭写端
    void shutdownWrite();

private:
    const int fd_;
};
