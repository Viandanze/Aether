#include "net/EventLoop.h"
#include "net/InetAddress.h"
#include "http/HttpServer.h"
#include "base/Logger.h"
#include <atomic>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fstream>
#include <sys/stat.h>

// ─── Global variables (for signal handling)───
std::atomic<bool> g_running{false};
EventLoop* g_loop = nullptr;
HttpServer* g_server = nullptr;

// ─── Signal handling: graceful Close ───
void installSignalHandlers() {
    ::signal(SIGPIPE, SIG_IGN);  // Ignore SIGPIPE (writing to closed socket)

    // SIGINT/SIGTERM: graceful Close
    auto handler = [](int sig) {
        if (!g_running.load(std::memory_order_relaxed)) {
            // Server not fully started yet, ignore signal
            return;
        }
        LOG_INFO("Received signal %d, starting graceful shutdown...", sig);
        if (g_server && g_loop) {
            // Stop accepting new connections
            g_server->getTcpServer()->stopAccepting();

            // If no active connections, quit immediately
            if (g_server->getTcpServer()->connectionCount() == 0) {
                g_loop->queueInLoop([]() { g_loop->quit(); });
                return;
            }

            // Wait 5 seconds to let connections finish
            g_loop->runAfter(5.0, []() {
                int remaining = g_server->getTcpServer()->connectionCount();
                if (remaining > 0) {
                    LOG_WARN("Graceful shutdown timeout, force closing %d connections", remaining);
                    g_server->getTcpServer()->forceCloseAll();
                    g_loop->runAfter(0.5, []() { g_loop->quit(); });
                } else {
                    g_loop->quit();
                }
            });
        }
    };

    ::signal(SIGINT, handler);
    ::signal(SIGTERM, handler);
}

// ─── MIME type detection ───
std::string getMimeType(const std::string& path) {
    auto dot = path.rfind('.');
    if (dot == std::string::npos) return "application/octet-stream";

    std::string ext = path.substr(dot);
    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css")  return "text/css; charset=utf-8";
    if (ext == ".js")   return "application/javascript; charset=utf-8";
    if (ext == ".json") return "application/json; charset=utf-8";
    if (ext == ".png")  return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif")  return "image/gif";
    if (ext == ".svg")  return "image/svg+xml";
    if (ext == ".ico")  return "image/x-icon";
    if (ext == ".txt")  return "text/plain; charset=utf-8";
    if (ext == ".xml")  return "text/xml; charset=utf-8";
    if (ext == ".pdf")  return "application/pdf";
    if (ext == ".woff" || ext == ".woff2") return "font/woff";
    if (ext == ".ttf")  return "font/ttf";
    return "application/octet-stream";
}

// ─── Path safety check ───
bool isPathSafe(const std::string& path) {
    // Block path traversal attacks
    // Check for ".." in URL path components
    if (path.find("..") != std::string::npos) return false;
    // Block null bytes and control characters
    for (char c : path) {
        if (c <= 0x1f || c == 0x7f) return false;
    }
    return true;
}

// ─── HTTP request handling ───
void onRequest(const HttpRequest& req, HttpResponse* resp) {
    // Only support GET/HEAD/POST
    auto method = req.method();
    if (method != HttpRequest::kGet &&
        method != HttpRequest::kHead &&
        method != HttpRequest::kPost) {
        *resp = HttpResponse::methodNotAllowed();
        return;
    }

    const std::string& path = req.path();

    // Built-in API
    if (path == "/api/status") {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json; charset=utf-8");
        resp->setBody("{\"status\":\"running\",\"version\":\"0.5\",\"protocol\":\"HTTP/1.1\"}");
        return;
    }

    // POST echo
    if (method == HttpRequest::kPost) {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json; charset=utf-8");
        resp->setBody("{\"method\":\"POST\",\"path\":\"" + path +
                       "\",\"body_length\":" + std::to_string(req.body().size()) + "}");
        return;
    }

    // Static file serving (GET/HEAD)
    if (!isPathSafe(path)) {
        *resp = HttpResponse::forbidden();
        return;
    }

    std::string serveDir = "./www";
    std::string filePath = serveDir + (path == "/" ? "/index.html" : path);

    struct stat st;
    if (::stat(filePath.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) {
        *resp = HttpResponse::notFound();
        return;
    }

    // Verify resolved path is within serve directory (defend against symlinks/encoding)
    char realPathBuf[PATH_MAX];
    if (::realpath(filePath.c_str(), realPathBuf) != nullptr) {
        char realDirBuf[PATH_MAX];
        if (::realpath(serveDir.c_str(), realDirBuf) != nullptr) {
            std::string resolvedFile(realPathBuf);
            std::string resolvedDir(realDirBuf);
            if (resolvedFile.find(resolvedDir + "/") != 0 && resolvedFile != resolvedDir) {
                *resp = HttpResponse::forbidden();
                return;
            }
        }
    }

    // Read file
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        *resp = HttpResponse::notFound();
        return;
    }

    std::string body((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());

    resp->setStatusCode(HttpResponse::k200Ok);
    resp->setStatusMessage("OK");
    resp->setContentType(getMimeType(filePath));
    if (method == HttpRequest::kGet) {
        resp->setBody(std::move(body));
    }
    // HEAD request: return headers only, no body
}

int main(int argc, char* argv[]) {
    uint16_t port = 8080;
    int numThreads = 0;
    int maxConn = 0;
    double idleTimeout = 30;
    std::string serveDir = "./www";
    bool asyncLog = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            port = static_cast<uint16_t>(atoi(argv[++i]));
        } else if (arg == "-t" && i + 1 < argc) {
            numThreads = atoi(argv[++i]);
        } else if (arg == "-m" && i + 1 < argc) {
            maxConn = atoi(argv[++i]);
        } else if (arg == "-i" && i + 1 < argc) {
            idleTimeout = atof(argv[++i]);
        } else if (arg == "-d" && i + 1 < argc) {
            serveDir = argv[++i];
        } else if (arg == "-l" && i + 1 < argc) {
            std::string level = argv[++i];
            if (level == "debug") Logger::instance().setLevel(LogLevel::DEBUG);
            else if (level == "warn") Logger::instance().setLevel(LogLevel::WARN);
            else if (level == "error") Logger::instance().setLevel(LogLevel::ERROR);
        } else if (arg == "--async-log") {
            asyncLog = true;
        } else if (arg == "-h" || arg == "--help") {
            printf("Aether HTTP Server v0.5\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  -p <port>      Listen port (default: 8080)\n");
            printf("  -t <threads>   IO thread count (default: 0=single reactor)\n");
            printf("  -m <max_conn>  Max connections (default: 0=unlimited)\n");
            printf("  -i <seconds>   Idle timeout (default: 30, 0=disabled)\n");
            printf("  -d <dir>       Serve directory (default: ./www)\n");
            printf("  -l <level>     Log level: debug/info/warn/error (default: info)\n");
            printf("  --async-log    Enable async file logging\n");
            printf("  -h             Show this help\n");
            return 0;
        } else {
            // Compatible with legacy positional args
            static int posArg = 0;
            switch (posArg) {
                case 0: port = static_cast<uint16_t>(atoi(argv[i])); break;
                case 1: numThreads = atoi(argv[i]); break;
                case 2: serveDir = argv[i]; break;
            }
            posArg++;
        }
    }

    // ─── Enable async logging ───
    if (asyncLog) {
        Logger::instance().enableAsync();
    }

    LOG_INFO("════════════════════════════════════════════════");
    LOG_INFO("  Aether HTTP Server v0.5");
    LOG_INFO("  Listening on 0.0.0.0:%d", port);
    LOG_INFO("  IO threads: %s", numThreads > 0 ? std::to_string(numThreads).c_str() : "1 (single Reactor)");
    LOG_INFO("  Max connections: %s", maxConn > 0 ? std::to_string(maxConn).c_str() : "unlimited");
    LOG_INFO("  Idle timeout: %gs (TimerWheel)", idleTimeout);
    LOG_INFO("  Serve dir: %s", serveDir.c_str());
    LOG_INFO("  Async logging: %s", asyncLog ? "ON" : "OFF");
    LOG_INFO("════════════════════════════════════════════════");

    // ─── Create server ───
    EventLoop loop;
    g_loop = &loop;
    installSignalHandlers();

    InetAddress listenAddr(port);
    HttpServer server(&loop, listenAddr, "Aether-HTTP");
    g_server = &server;

    server.setHttpCallback(onRequest);
    if (numThreads > 0) {
        server.setThreadNum(numThreads);
    }
    if (maxConn > 0) {
        server.setMaxConnections(maxConn);
    }
    if (idleTimeout > 0) {
        server.setIdleTimeout(static_cast<int>(idleTimeout));
    }

    server.start();

    LOG_INFO("Server running. Press Ctrl+C for graceful shutdown.");
    g_running.store(true, std::memory_order_release);
    loop.loop();

    LOG_INFO("Event loop exited, server shutdown complete.");
    g_running.store(false, std::memory_order_release);
    g_loop = nullptr;
    g_server = nullptr;

    return 0;
}
