#include "net/EventLoop.h"
#include "net/Channel.h"
#include "net/InetAddress.h"
#include "http/HttpServer.h"
#include "http/FileCache.h"
#include "base/Logger.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <sys/eventfd.h>
#include <unistd.h>

// --- globals (for signal handling) ---
EventLoop* g_loop = nullptr;
HttpServer* g_server = nullptr;
static int g_shutdownFd = -1;                    // SIGINT writes to this eventfd to wake the main loop
static std::atomic<bool> g_shuttingDown{false};  // guards graceful shutdown against re-entry

// --- graceful shutdown (runs in normal context on the main loop thread) ---
static void startGracefulShutdown() {
    // second shutdown signal: the user is impatient, exit immediately
    if (g_shuttingDown.load()) {
        LOG_WARN("Second shutdown signal, exiting immediately");
        ::_exit(130);
    }
    g_shuttingDown.store(true);

    LOG_INFO("Starting graceful shutdown: stop accepting, drain connections...");
    g_server->getTcpServer()->stopAccepting();

    // no live connections, exit now
    if (g_server->getTcpServer()->connectionCount() == 0) {
        g_loop->quit();
        return;
    }

    // wait up to 5s for in-flight requests, then force-close what remains
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

// --- signal handling ---
// Only async-signal-safe functions may run inside a signal handler.
// The old handler called stopAccepting/runAfter directly (both touch mutexes and logging);
// if the signal interrupted a path holding that same lock (TimerQueue/Logger), the process
// self-deadlocked (seen as no response after SIGINT, needing a 60s kill -9). Fix: the handler
// now only write(2)s an eventfd; the main loop wakes up and runs the full shutdown sequence in normal context.
void installSignalHandlers() {
    ::signal(SIGPIPE, SIG_IGN);  // ignore SIGPIPE (write to a closed socket)

    auto handler = [](int) {
        // async-signal-safe: write(2) only
        if (g_shutdownFd >= 0) {
            uint64_t one = 1;
            ssize_t n = ::write(g_shutdownFd, &one, sizeof(one));
            (void)n;
        }
    };
    ::signal(SIGINT, handler);
    ::signal(SIGTERM, handler);
}

// --- MIME type detection ---
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

// --- path safety check ---
bool isPathSafe(const std::string& path) {
    // block path traversal attacks
    if (path.find("..") != std::string::npos) return false;
    return true;
}

// --- HTTP request handling ---
void onRequest(const HttpRequest& req, HttpResponse* resp) {
    // only GET/HEAD/POST are supported
    auto method = req.method();
    if (method != HttpRequest::kGet &&
        method != HttpRequest::kHead &&
        method != HttpRequest::kPost) {
        *resp = HttpResponse::methodNotAllowed();
        return;
    }

    const std::string& path = req.path();

    // built-in API
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

    // static file serving (GET/HEAD): via FileCache (LRU + mtime/size validation + ETag/304)
    if (!isPathSafe(path)) {
        *resp = HttpResponse::forbidden();
        return;
    }

    FileCache::instance().serve(req, resp);
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
            // legacy positional arguments
            static int posArg = 0;
            switch (posArg) {
                case 0: port = static_cast<uint16_t>(atoi(argv[i])); break;
                case 1: numThreads = atoi(argv[i]); break;
                case 2: serveDir = argv[i]; break;
            }
            posArg++;
        }
    }

    // --- enable async logging ---
    if (asyncLog) {
        Logger::instance().enableAsync();
    }

    // --- static file cache (the -d option takes effect here) ---
    FileCache::instance().init(serveDir);

    LOG_INFO("════════════════════════════════════════════════");
    LOG_INFO("  Aether HTTP Server v0.5");
    LOG_INFO("  Listening on 0.0.0.0:%d", port);
    LOG_INFO("  IO threads: %s", numThreads > 0 ? std::to_string(numThreads).c_str() : "1 (single Reactor)");
    LOG_INFO("  Max connections: %s", maxConn > 0 ? std::to_string(maxConn).c_str() : "unlimited");
    LOG_INFO("  Idle timeout: %gs (TimerWheel)", idleTimeout);
    LOG_INFO("  Serve dir: %s", serveDir.c_str());
    LOG_INFO("  Async logging: %s", asyncLog ? "ON" : "OFF");
    LOG_INFO("════════════════════════════════════════════════");

    // --- create the server ---
    EventLoop loop;
    g_loop = &loop;

    // graceful shutdown signal channel: the handler writes an eventfd (async-signal-safe),
    // the main loop reads it and runs the shutdown sequence in normal context.
    // Declaration order guarantees: server destructs first (joins IO threads), shutdownChannel destructs before loop.
    g_shutdownFd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    Channel shutdownChannel(&loop, g_shutdownFd);
    shutdownChannel.setReadCallback([]() {
        uint64_t n;
        while (::read(g_shutdownFd, &n, sizeof(n)) == sizeof(n)) {}
        startGracefulShutdown();
    });
    shutdownChannel.enableReading();

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
    loop.loop();

    LOG_INFO("Event loop exited, server shutdown complete.");
    g_loop = nullptr;
    g_server = nullptr;

    return 0;
}
