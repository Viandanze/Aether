#ifndef AETHER_HTTP_HTTPSERVER_H
#define AETHER_HTTP_HTTPSERVER_H
#pragma once
#include <functional>
#include <memory>
#include <string>
#include "net/TcpServer.h"
#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"

/// HttpServer: HTTP server built on TcpServer
///
/// Supports:
/// - HTTP/1.1 Keep-Alive
/// - HTTP pipelining (multiple requests sent back-to-back on one connection)
/// - Chunked Transfer-Encoding
/// - idle connection timeout (TimerWheel)
class HttpServer {
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(EventLoop* loop, const InetAddress& listenAddr,
               const std::string& name, bool reusePort = true);
    ~HttpServer();

    EventLoop* getLoop() const { return server_.getLoop(); }
    TcpServer* getTcpServer() { return &server_; }

    void setHttpCallback(HttpCallback cb) { httpCallback_ = std::move(cb); }
    void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }
    void setMaxConnections(int maxConn) { server_.setMaxConnections(maxConn); }
    void setIdleTimeout(int seconds) { server_.setIdleTimeout(seconds); }

    void start();

private:
    void onConnection(const std::shared_ptr<TcpConnection>& conn);
    void onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf);
    void onRequest(const std::shared_ptr<TcpConnection>& conn, const HttpRequest& req);

    TcpServer server_;
    HttpCallback httpCallback_;
};
#endif // AETHER_HTTP_HTTPSERVER_H
