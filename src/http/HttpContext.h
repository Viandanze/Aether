#ifndef AETHER_HTTP_HTTPCONTEXT_H
#define AETHER_HTTP_HTTPCONTEXT_H
#pragma once
#include <string_view>
#include "HttpRequest.h"

class Buffer;

/// HttpContext: per-connection HTTP parsing state machine
///
/// Handles sticky/half packets: the buffer may hold partial or multiple requests
///
/// State transitions:
///   ExpectRequestLine → ExpectHeaders → ExpectBody → GotComplete
///                                     → ExpectChunkSize → ExpectChunkData → ... → GotComplete
///
/// Supports two body transfer modes:
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

    /// Parse an HTTP request from the Buffer
    /// Successfully parsed bytes are consumed from the Buffer (retrieve)
    /// Returns false on parse error; the connection should be closed
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
#endif // AETHER_HTTP_HTTPCONTEXT_H
