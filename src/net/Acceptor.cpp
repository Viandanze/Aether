#include "Acceptor.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Socket.h"
#include "Channel.h"
#include "base/Logger.h"
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reusePort)
    : loop_(loop),
      acceptSocket_(new Socket(::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0))),
      acceptChannel_(new Channel(loop, acceptSocket_->fd())),
      listening_(false),
      idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) {
    acceptSocket_->setReuseAddr(true);
    acceptSocket_->setReusePort(reusePort);
    acceptSocket_->bindAddress(listenAddr);
    acceptChannel_->setReadCallback([this]() { handleRead(); });
}

Acceptor::~Acceptor() {
    acceptChannel_->disableAll();
    acceptChannel_->remove();
    ::close(idleFd_);
}

void Acceptor::listen() {
    loop_->assertInLoopThread();
    listening_ = true;
    acceptSocket_->listen();
    acceptChannel_->enableReading();
}

void Acceptor::stopListening() {
    loop_->assertInLoopThread();
    listening_ = false;
    acceptChannel_->disableReading();
    LOG_INFO("Acceptor: stopped listening");
}

void Acceptor::handleRead() {
    InetAddress peerAddr;
    int connFd = acceptSocket_->accept(&peerAddr);
    if (connFd >= 0) {
        if (newConnectionCallback_) {
            newConnectionCallback_(connFd, peerAddr);
        } else {
            ::close(connFd);
        }
    } else {
        if (errno == EMFILE) {
            ::close(idleFd_);
            idleFd_ = ::accept(acceptSocket_->fd(), nullptr, nullptr);
            ::close(idleFd_);
            idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        }
        LOG_ERROR("Acceptor::handleRead accept error: %s", strerror(errno));
    }
}
