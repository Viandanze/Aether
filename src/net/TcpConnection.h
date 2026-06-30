#pragma once
#include <string>
#include <memory>
#include <functional>
#include <any>
#include <cstdint>
#include "base/noncopyable.h"
#include "net/Buffer.h"

class EventLoop;
class Socket;
class Channel;
class InetAddress;

/// TcpConnection: manages an established TCP connection
///
/// Lifecycle: Connecting -> Connected -> Disconnecting -> Disconnected
/// Managed with shared_ptr to prevent destruction during callbacks
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    using ConnectionCallback    = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    using MessageCallback       = std::function<void(const std::shared_ptr<TcpConnection>&, Buffer*)>;
    using WriteCompleteCallback = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    using CloseCallback         = std::function<void(const std::shared_ptr<TcpConnection>&)>;
    using HighWaterMarkCallback = std::function<void(const std::shared_ptr<TcpConnection>&, size_t)>;

    TcpConnection(EventLoop* loop, const std::string& name, int fd,
                  const InetAddress& localAddr, const InetAddress& peerAddr);
    ~TcpConnection();

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddr() const { return localAddr_; }
    const InetAddress& peerAddr() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    bool disconnected() const { return state_ == kDisconnected; }

    // ─── Send ───
    void send(const std::string& message);
    void send(const char* data, size_t len);
    void send(Buffer* buf);  // Zero-copy swap

    // ─── Close ───
    void shutdown();       // Half-close (close write end after flushing pending data)
    void forceClose();     // Immediate close
    void forceCloseWithDelay(double seconds);  // Delayed force close

    // ─── Callbacks ───
    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }
    void setHighWaterMarkCallback(HighWaterMarkCallback cb, size_t highWaterMark) {
        highWaterMarkCallback_ = std::move(cb);
        highWaterMark_ = highWaterMark;
    }

    // ─── Context ───
    void setContext(const std::any& ctx) { context_ = ctx; }
    const std::any& getContext() const { return context_; }
    std::any* getMutableContext() { return &context_; }

    // ─── Buffer access ───
    Buffer* inputBuffer() { return &inputBuf_; }
    Buffer* outputBuffer() { return &outputBuf_; }

    // ─── Idle timeout (for TimerWheel)───
    uint64_t idleTimerGeneration() const { return idleTimerGeneration_; }
    uint64_t& idleTimerGeneration() { return idleTimerGeneration_; }

    // ─── TCP options ───
    void setTcpNoDelay(bool on);

    // ─── Connection establish/destroy (internal)───
    void connectEstablished();
    void connectDestroyed();

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

    void setState(StateE s) { state_ = s; }
    void handleRead();
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const std::string& message);
    void sendInLoop(const char* data, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();

    EventLoop* loop_;
    std::string name_;
    StateE state_;
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;
    InetAddress localAddr_;
    InetAddress peerAddr_;

    Buffer inputBuf_;   // Read buffer
    Buffer outputBuf_;  // Write buffer

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    size_t highWaterMark_;

    std::any context_;  // Connection-level context (e.g. HttpContext)

    uint64_t idleTimerGeneration_;  // For TimerWheel, incremented on refresh to invalidate old entries
};
