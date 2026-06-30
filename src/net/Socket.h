#pragma once
#include "base/noncopyable.h"

// RAII wrapper for socket fd
class Socket : noncopyable {
public:
    explicit Socket(int fd) : fd_(fd) {}
    ~Socket();

    int fd() const { return fd_; }

    // Server-side socket operations
    void bindAddress(const class InetAddress& localAddr);
    void listen();
    int  accept(class InetAddress* peerAddr);

    // Socket options
    void setReuseAddr(bool on);
    void setReusePort(bool on);
    void setTcpNoDelay(bool on);
    void setKeepAlive(bool on);

    // Shutdown write end
    void shutdownWrite();

private:
    const int fd_;
};
