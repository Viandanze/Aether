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

        // Insert into TimerWheel on first access (if idle timeout configured)
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
            // Continue parsing remaining requests in buffer (pipelining)
        } else {
            break;  // Request incomplete, wait for more data
        }
    }

    // Refresh idle timeout (connection is active)
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
