#include "HttpServer.h"
#include "net/TcpConnection.h"
#include "base/Logger.h"

HttpServer::HttpServer(EventLoop* loop, const InetAddress& listenAddr,
                       const std::string& name, bool reusePort)
    : server_(loop, listenAddr, name) {
    server_.setConnectionCallback([this](const std::shared_ptr<TcpConnection>& conn) {
        onConnection(conn);
    });
    server_.setMessageCallback([this](const std::shared_ptr<TcpConnection>& conn, Buffer* buf) {
        onMessage(conn, buf);
    });
}

HttpServer::~HttpServer() {}

void HttpServer::start() {
    server_.start();
}

void HttpServer::onConnection(const std::shared_ptr<TcpConnection>& conn) {
    if (conn->connected()) {
        conn->setContext(HttpContext());
        LOG_INFO("HttpServer: new connection from %s", conn->peerAddr().toIpPort().c_str());

        // 首次插入TimerWheel（如果配置了空闲超时）
        conn->getLoop()->insertToWheel(conn);
    } else {
        LOG_INFO("HttpServer: connection closed from %s", conn->peerAddr().toIpPort().c_str());
    }
}

void HttpServer::onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf) {
    auto* ctx = std::any_cast<HttpContext>(conn->getMutableContext());

    // HTTP Pipelining: loop to parse all requests in buffer
    while (buf->readableBytes() > 0) {
        bool ok = ctx->parseRequest(buf);
        if (!ok) {
            conn->send(HttpResponse::badRequest().serialize());
            conn->shutdown();
            return;
        }

        if (ctx->gotComplete()) {
            onRequest(conn, ctx->request());
            ctx->reset();
            // 继续解析buffer中可能剩余的请求（pipelining）
        } else {
            break;  // 请求不完整，等待更多数据
        }
    }

    // 刷新空闲超时（连接有活动）
    conn->getLoop()->insertToWheel(conn);
}

void HttpServer::onRequest(const std::shared_ptr<TcpConnection>& conn, const HttpRequest& req) {
    bool close = !req.isKeepAlive();

    HttpResponse resp(close);
    if (httpCallback_) {
        httpCallback_(req, &resp);
    }

    std::string output = resp.serialize();
    conn->send(output);

    if (resp.closeConnection()) {
        conn->shutdown();
    }
}
