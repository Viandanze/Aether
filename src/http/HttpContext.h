#pragma once
#include <string_view>
#include "HttpRequest.h"

class Buffer;

/// HttpContext: connection-level HTTP parsing state machine
///
/// Handle sticky/half packets: buffer may contain incomplete or multiple requests
///
/// State transitions:
///   ExpectRequestLine → ExpectHeaders → ExpectBody → GotComplete
///                                     → ExpectChunkSize → ExpectChunkData → ... → GotComplete
///
/// Supports two Body transfer modes:
///   1. Content-Length (fixed length)
///   2. Transfer-Encoding: chunked
class HttpContext {
public:
    enum ParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kExpectChunkSize,
        kExpectChunkData,
        kExpectChunkFinal,
        kGotComplete,
    };

    HttpContext() : state_(kExpectRequestLine), chunkSize_(0) {}

    /// Parse HTTP request from Buffer
    /// Successfully parsed portion auto-consumed (retrieved) from Buffer
    /// Returns false on parse error, connection should be closed
    bool parseRequest(Buffer* buf);

    bool gotComplete() const { return state_ == kGotComplete; }
    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }

    void reset() {
        state_ = kExpectRequestLine;
        request_ = HttpRequest();
        chunkSize_ = 0;
    }

private:
    bool processRequestLine(std::string_view line);
    bool processChunkSize(std::string_view line);

    ParseState state_;
    HttpRequest request_;
    size_t chunkSize_;
};
