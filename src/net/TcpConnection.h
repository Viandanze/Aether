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

/// TcpConnection：管理一个已建立的TCP连接
///
/// 生命周期：Connecting → Connected → Disconnecting → Disconnected
/// 使用shared_ptr管理，确保回调期间连接不会被销毁
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

    // ─── 发送 ───
    void send(const std::string& message);
    void send(const char* data, size_t len);
    void send(Buffer* buf);  // 零拷贝swap

    // ─── 关闭 ───
    void shutdown();       // 半关闭（发完待写数据后关闭写端）
    void forceClose();     // 立即关闭
    void forceCloseWithDelay(double seconds);  // Delayed force close

    // ─── 回调设置 ───
    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }
    void setHighWaterMarkCallback(HighWaterMarkCallback cb, size_t highWaterMark) {
        highWaterMarkCallback_ = std::move(cb);
        highWaterMark_ = highWaterMark;
    }

    // ─── 上下文 ───
    void setContext(const std::any& ctx) { context_ = ctx; }
    const std::any& getContext() const { return context_; }
    std::any* getMutableContext() { return &context_; }

    // ─── Buffer访问 ───
    Buffer* inputBuffer() { return &inputBuf_; }
    Buffer* outputBuffer() { return &outputBuf_; }

    // ─── Idle timeout (for TimerWheel) ───
    uint64_t idleTimerGeneration() const { return idleTimerGeneration_; }
    uint64_t& idleTimerGeneration() { return idleTimerGeneration_; }

    // ─── TCP选项 ───
    void setTcpNoDelay(bool on);

    // ─── 连接建立/销毁（内部调用）───
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

    Buffer inputBuf_;   // 读缓冲
    Buffer outputBuf_;  // 写缓冲

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
    HighWaterMarkCallback highWaterMarkCallback_;
    size_t highWaterMark_;

    std::any context_;  // 连接级上下文（如HttpContext）

    uint64_t idleTimerGeneration_;  // TimerWheel用，每次刷新递增使旧条目失效
};
