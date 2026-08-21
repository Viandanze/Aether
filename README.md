# Aether

High-performance HTTP server built with C++17, featuring a master-slave Reactor architecture with epoll edge-triggered I/O.

## Architecture

```
                    ┌─────────────────────┐
                    │    Main Reactor     │
                    │  (Acceptor + epoll) │
                    └────────┬────────────┘
                             │ new connection
                    ┌────────▼────────────┐
                    │  Round-Robin Dispatch│
                    └──┬─────┬─────┬──────┘
                       │     │     │
               ┌───────▼┐ ┌──▼───┐ ┌▼───────┐
               │Sub IO-0│ │IO-1  │ │IO-N    │
               │Loop+Wheel│ │Loop+Wheel│ │Loop+Wheel│
               └────────┘ └──────┘ └────────┘
```

## Features

- **Master-Slave Reactor**: Acceptor in main thread, IO threads for connections
- **epoll ET Mode**: Edge-triggered non-blocking I/O with readv zero-copy read
- **Buffer**: Auto-growth buffer with readv + stack extrabuf, prepend support
- **HTTP/1.1 Parser**: Finite state machine with half-packet / sticky-packet support
- **HTTP Pipelining**: Loop-parse multiple requests per connection
- **Chunked Transfer-Encoding**: Full support for chunked request bodies
- **Keep-Alive**: HTTP/1.1 persistent connections by default
- **TimerWheel**: O(1) idle connection timeout with generation-based invalidation
- **Connection Limiter**: Configurable max connections to prevent DDoS
- **Async Logger**: Dual-buffer async logging with date+size rotation
- **Log Rotation**: Auto-roll at midnight + 100MB size limit + keep N old files
- **Graceful Shutdown**: SIGINT → stop accepting → drain connections → force close
- **High Water Mark**: Callback when output buffer exceeds threshold
- **Static File Server**: MIME type detection, path traversal protection
- **FileCache**: LRU in-memory file cache with mtime+size validation, ETag / If-None-Match 304 support, large-file bypass (>4MB not cached), 64MB total cap
- **sendfile(2) Zero-Copy**: Large files (>4MB) streamed directly from page cache to socket via `sendfile(2)` — no user-space copy; fd lifetime scoped inside the send function (no member state, no leak on mid-transfer disconnect); EAGAIN fallback to `pread` + output buffer; HEAD requests report true Content-Length (RFC 9110)
- **POST Support**: Request body parsing with Content-Length and chunked encoding
- **Built-in API**: `/api/status` endpoint for server health checks
- **Live Metrics**: `/api/stats` runtime endpoint — uptime, active connections, total request counter (atomic, all IO threads), FileCache hits/misses/bypass counters; consumed by `www/dashboard.html`, a zero-dependency oscilloscope-style live monitor (QPS chart, cache hit ring, connection stats)
- **Thread-Safe**: Cross-thread task dispatch via `runInLoop()` + `eventfd` wakeup

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Run

```bash
# Show help
./aether -h

# Single Reactor mode (default)
./aether -p 8080

# Multi-Reactor mode (4 IO threads)
./aether -p 8080 -t 4

# With all options
./aether -p 8080 -t 4 -m 10000 -i 30 -d ./public --async-log

# Legacy positional args (backward compatible)
./aether 8080 4 ./public
```

Then open the live metrics dashboard (zero external dependencies, works offline):

```
http://localhost:8080/dashboard.html
```

### Command Line Options

| Option | Description | Default |
|--------|-------------|---------|
| `-p <port>` | Listen port | 8080 |
| `-t <threads>` | IO thread count (0=single reactor) | 0 |
| `-m <max_conn>` | Max connections (0=unlimited) | 0 |
| `-i <seconds>` | Idle timeout (0=disabled, TimerWheel) | 30 |
| `-d <dir>` | Serve directory | ./www |
| `-l <level>` | Log level: debug/info/warn/error | info |
| `--async-log` | Enable async file logging | off |

## Test

```bash
# Unit tests (148 assertions across 5 suites)
cd test && make && make test

# Basic GET request
curl http://localhost:8080/

# Server status API
curl http://localhost:8080/api/status

# POST request
curl -X POST -d "hello world" http://localhost:8080/

# Keep-alive test
curl -v http://localhost:8080/

# HTTP Pipelining (two requests in one TCP send)
(echo -e "GET /api/status HTTP/1.1\r\nHost: localhost\r\n\r\nGET / HTTP/1.1\r\nHost: localhost\r\n\r\n") | nc localhost 8080

# Chunked transfer encoding
curl -X POST -H "Transfer-Encoding: chunked" -d "5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n" http://localhost:8080/

# Graceful shutdown test
kill -SIGINT <pid>
```

## Project Structure

```
src/
├── base/
│   ├── Logger.h/.cpp           # Logging system (sync + async)
│   ├── AsyncLogger.h/.cpp      # Async dual-buffer logger + rotation
│   ├── Timer.h/.cpp            # Timer abstraction
│   ├── TimerId.h               # Timer handle for cancellation
│   ├── TimerQueue.h/.cpp       # timerfd-based timer queue
│   ├── TimerWheel.h/.cpp       # O(1) idle timeout wheel
│   ├── TimeStamp.h             # Time utility
│   └── noncopyable.h           # Non-copyable base
├── net/
│   ├── Buffer.h                # Auto-growth buffer with readv
│   ├── EventLoop.h/.cpp        # Event loop + timer + TimerWheel
│   ├── Epoller.h/.cpp          # epoll ET wrapper
│   ├── Channel.h/.cpp          # Event dispatcher per fd
│   ├── Acceptor.h/.cpp         # New connection acceptor
│   ├── TcpServer.h/.cpp        # TCP server + graceful shutdown
│   ├── TcpConnection.h/.cpp    # TCP connection + Buffer + high water mark + sendfile
│   ├── EventLoopThread.h/.cpp  # IO thread wrapper
│   ├── EventLoopThreadPool.h/.cpp  # Sub-reactor thread pool
│   ├── InetAddress.h/.cpp      # Socket address wrapper
│   └── Socket.h/.cpp           # RAII socket
└── http/
    ├── HttpRequest.h/.cpp      # HTTP request struct
    ├── HttpResponse.h/.cpp     # HTTP response builder (+ zero-copy file body)
    ├── HttpContext.h/.cpp      # HTTP parse state machine (chunked + Buffer)
    ├── HttpServer.h/.cpp       # HTTP server + pipelining
    └── FileCache.h             # LRU static file cache (mtime+size validation, ETag/304)
```

## Benchmark

wrk 4.1.0, Ubuntu 22.04, g++ 11 `-O2` Release. **Single vCPU sandbox** — server
and load generator share one core, so absolute numbers are for engineering
judgement, not headline claims.

| Scenario (10s, keep-alive) | Before FileCache | After FileCache | Gain |
|----------------------------|-----------------:|----------------:|-----:|
| GET `/` (1850B), 2 threads, c50 | 170.62 QPS | 99,108.38 QPS | 581× |
| GET `/` (1850B), 1 reactor, c50 | 88.66 QPS | 100,008.77 QPS | 1,128× |
| GET `/api/status`, 2 threads, c50 | 109,581.86 QPS | 125,918.59 QPS | +15% |

Post-optimization latency (2 IO threads, c50): `GET /` P50 536µs / P99 1.47ms;
`/api/status` P50 380µs / P99 785µs. Zero socket errors after optimization
(before: 21–85 errors per scenario). Root cause of the gap: every static-file
request previously hit the filesystem (stat + open + read + close); FileCache
serves hot files from memory with mtime+size freshness checks.

Large-file path (8MB file, single vCPU sandbox):

| Metric | FileCache bypass (pread) | sendfile(2) zero-copy | Gain |
|--------|-------------------------:|----------------------:|-----:|
| Single-stream throughput (best of 3) | 141 MB/s | 218 MB/s | +55% |
| Sustained, 50 concurrent (wrk t2 c50) | — | 161.7 MB/s | — |
| Small-file / API QPS regression | — | none (99k / 126k, flat) | — |

sendfile eliminates two user-space copies (page cache → user buffer → socket
buffer) and one context-switch-heavy `write()` on the hot path; verification
covered md5 integrity, HEAD/304/404 semantics, keep-alive reuse across a
sendfile transfer, Connection:close ordering, 4-way concurrent downloads, and
mid-transfer client disconnect storms (EPIPE handled, zero fd leaks,
114/114 connections cleaned up).
