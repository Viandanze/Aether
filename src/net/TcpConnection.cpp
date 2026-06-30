#include "TcpConnection.h"
#include "EventLoop.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"
#include "base/Logger.h"
#include <unistd.h>
#include <errno.h>
#include <cstring>

TcpConnection::TcpConnection(EventLoop* loop, const std::string& name, int fd,
                             const InetAddress& localAddr, const InetAddress& peerAddr)
    : loop_(loop),
      name_(name),
      state_(kConnecting),
      socket_(new Socket(fd)),
      channel_(new Channel(loop, fd)),
      localAddr_(localAddr),
      peerAddr_(peerAddr),
      highWaterMark_(64 * 1024 * 1024),  // 64MB
      idleTimerGeneration_(0) {
    channel_->setReadCallback([this]() { handleRead(); });
    channel_->setWriteCallback([this]() { handleWrite(); });
    channel_->setCloseCallback([this]() { handleClose(); });
    channel_->setErrorCallback([this]() { handleError(); });
}

TcpConnection::~TcpConnection() {}

void TcpConnection::connectEstablished() {
    setState(kConnected);
    channel_->enableReading();
    if (connectionCallback_) connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed() {
    setState(kDisconnected);
    channel_->disableAll();
    if (connectionCallback_) connectionCallback_(shared_from_this());
    channel_->remove();
}

// ─── Send ───

void TcpConnection::send(const std::string& message) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(message);
        } else {
            loop_->runInLoop([this, message]() {
                sendInLoop(message);
            });
        }
    }
}

void TcpConnection::send(const char* data, size_t len) {
    send(std::string(data, len));
}

void TcpConnection::send(Buffer* buf) {
    if (state_ == kConnected) {
        if (loop_->isInLoopThread()) {
            sendInLoop(buf->retrieveAllAsString());
        } else {
            loop_->runInLoop([this, str = buf->retrieveAllAsString()]() {
                sendInLoop(str);
            });
        }
    }
}

void TcpConnection::sendInLoop(const std::string& message) {
    sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const char* data, size_t len) {
    ssize_t nwrote = 0;
    size_t remaining = len;

    // If output buffer empty and not watching write events, try direct write
    if (!channel_->isWriting() && outputBuf_.readableBytes() == 0) {
        nwrote = ::write(channel_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && writeCompleteCallback_) {
                loop_->queueInLoop([this]() { writeCompleteCallback_(shared_from_this()); });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop write error: %s", strerror(errno));
            }
        }
    }

    // Put unwritten data in outputBuf_, continue when fd is writable
    if (remaining > 0) {
        // High water mark check
        size_t oldLen = outputBuf_.readableBytes();
        if (oldLen + remaining >= highWaterMark_
            && oldLen < highWaterMark_
            && highWaterMarkCallback_) {
            size_t highLen = oldLen + remaining;
            loop_->queueInLoop([this, highLen]() {
                highWaterMarkCallback_(shared_from_this(), highLen);
            });
        }
        outputBuf_.append(data + nwrote, remaining);
        if (!channel_->isWriting()) {
            channel_->enableWriting();
        }
    }
}

// ─── Close ───

void TcpConnection::shutdown() {
    if (state_ == kConnected) {
        setState(kDisconnecting);
        loop_->runInLoop([this]() { shutdownInLoop(); });
    }
}

void TcpConnection::shutdownInLoop() {
    if (!channel_->isWriting()) {
        socket_->shutdownWrite();
    }
}

void TcpConnection::forceClose() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        loop_->runInLoop([this]() { forceCloseInLoop(); });
    }
}

void TcpConnection::forceCloseWithDelay(double seconds) {
    if (state_ == kConnected || state_ == kDisconnecting) {
        setState(kDisconnecting);
        auto self = shared_from_this();
        loop_->runAfter(seconds, [self]() { self->forceClose(); });
    }
}

void TcpConnection::forceCloseInLoop() {
    if (state_ == kConnected || state_ == kDisconnecting) {
        handleClose();
    }
}

void TcpConnection::setTcpNoDelay(bool on) {
    socket_->setTcpNoDelay(on);
}

// ─── Event handlers ───

void TcpConnection::handleRead() {
    // ET mode must loop read until EAGAIN
    int savedErrno = 0;
    ssize_t n = inputBuf_.readFd(channel_->fd(), &savedErrno);

    while (n > 0) {
        if (messageCallback_) {
            messageCallback_(shared_from_this(), &inputBuf_);
        }
        // Continue reading, ET mode may have more data
        n = inputBuf_.readFd(channel_->fd(), &savedErrno);
    }

    if (n == 0) {
        handleClose();
    } else if (n < 0) {
        if (savedErrno != EAGAIN && savedErrno != EWOULDBLOCK) {
            LOG_ERROR("TcpConnection::handleRead error: %s", strerror(savedErrno));
            handleError();
        }
        // EAGAIN is normal exit in ET mode, not an error
    }
}

void TcpConnection::handleWrite() {
    if (channel_->isWriting()) {
        ssize_t n = ::write(channel_->fd(),
                            outputBuf_.peek(),
                            outputBuf_.readableBytes());
        if (n > 0) {
            outputBuf_.retrieve(n);
            if (outputBuf_.readableBytes() == 0) {
                channel_->disableWriting();
                if (writeCompleteCallback_) {
                    loop_->queueInLoop([this]() { writeCompleteCallback_(shared_from_this()); });
                }
                if (state_ == kDisconnecting) {
                    shutdownInLoop();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite write error: %s", strerror(errno));
        }
    }
}

void TcpConnection::handleClose() {
    setState(kDisconnected);
    channel_->disableAll();

    // Notify upper layer of connection close
    if (closeCallback_) closeCallback_(shared_from_this());
    if (connectionCallback_) connectionCallback_(shared_from_this());
}

void TcpConnection::handleError() {
    int opt = 0;
    socklen_t len = sizeof(opt);
    ::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &opt, &len);
    LOG_ERROR("TcpConnection::handleError SO_ERROR=%d (%s)", opt, strerror(opt));
}
