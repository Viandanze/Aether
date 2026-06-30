---
AIGC:
    Label: "1"
    ContentProducer: 001191110102MACQD9K64018705
    ProduceID: 1716792653782763_0/project_7652301137504190747-files/README.md
    ReservedCode1: ""
    ContentPropagator: 001191110102MACQD9K64028705
    PropagateID: 1716792653782763#1782819436881
    ReservedCode2: ""
---
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
- **POST Support**: Request body parsing with Content-Length and chunked encoding
- **Built-in API**: `/api/status` endpoint for server health checks
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
│   ├── TcpConnection.h/.cpp    # TCP connection + Buffer + high water mark
│   ├── EventLoopThread.h/.cpp  # IO thread wrapper
│   ├── EventLoopThreadPool.h/.cpp  # Sub-reactor thread pool
│   ├── InetAddress.h/.cpp      # Socket address wrapper
│   └── Socket.h/.cpp           # RAII socket
└── http/
    ├── HttpRequest.h/.cpp      # HTTP request struct
    ├── HttpResponse.h/.cpp     # HTTP response builder
    ├── HttpContext.h/.cpp      # HTTP parse state machine (chunked + Buffer)
    └── HttpServer.h/.cpp       # HTTP server + pipelining
```

## Roadmap

- [x] Week 1-2: Network layer scaffold (epoll/Channel/EventLoop/TcpServer)
- [x] Week 3: HTTP parser + EventLoopThreadPool + static file serving
- [x] Week 4: Timer module + async logger + connection limit + graceful shutdown + chunked encoding
- [x] Week 5-6: Buffer + TimerWheel + log rotation + HTTP pipelining + graceful shutdown v2
- [ ] Week 7-8: GTest unit tests + wrk benchmarking + perf analysis

## Interview Highlights

| Code Location | Must-Ask Question |
|---------------|-------------------|
| `Buffer.h` | Why readv+extrabuf? Why not just one big buffer? makeSpace strategy? |
| `Epoller.cpp` | epoll LT vs ET? Why ET needs non-blocking loop-read? epoll vs select? |
| `EventLoop.cpp` | eventfd vs pipe for wakeup? Why swap in doPendingFunctors? timerfd? |
| `TimerQueue.cpp` | Why timerfd not timer_create? How to handle cancel during callback? |
| `TimerWheel.h` | O(1) vs O(logN) TimerQueue? Generation-based invalidation? Slot cleanup? |
| `Acceptor.cpp` | EMFILE handling? idleFd trick? SO_REUSEPORT? |
| `TcpConnection.cpp` | Buffer vs vector<char>? High water mark? forceClose vs shutdown? |
| `TcpServer.cpp` | Round-Robin dispatch? Graceful shutdown flow? Thread safety of connections_? |
| `HttpContext.cpp` | FSM design? Half-packet/sticky-packet? Buffer integration? |
| `HttpServer.cpp` | Pipelining loop-parse? Why retrieve after each request? |
| `AsyncLogger.cpp` | Double buffering? Date+size rotation? Why swap not copy? |

---

> 本内容由 Coze AI 生成，请遵循相关法律法规及《人工智能生成合成内容标识办法》使用与传播。
