#include "HttpServer.h"
#include "net/TcpConnection.h"
#include "net/EventLoop.h"
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

        // first insert into the TimerWheel (if idle timeout is configured)
        conn->getLoop()->insertToWheel(conn);
    } else {
        LOG_INFO("HttpServer: connection closed from %s", conn->peerAddr().toIpPort().c_str());
    }
}

void HttpServer::onMessage(const std::shared_ptr<TcpConnection>& conn, Buffer* buf) {
    auto* ctx = std::any_cast<HttpContext>(conn->getMutableContext());

    // HTTP pipelining: parse every request left in the buffer
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
            // keep parsing requests that may remain in the buffer (pipelining)
        } else {
            break;  // incomplete request, wait for more data
        }
    }

    // refresh the idle timer (connection is active)
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

    // zero-copy file body: headers are already queued; the body is pushed by sendfile(2)
    // (fd ownership moves from resp to the connection)
    if (resp.hasFileBody()) {
        conn->sendFile(resp.takeFileFd(), resp.fileOffset(), resp.fileSize());
    }

    if (resp.closeConnection()) {
        conn->shutdown();
    }
}
