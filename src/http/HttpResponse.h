#ifndef AETHER_HTTP_HTTPRESPONSE_H
#define AETHER_HTTP_HTTPRESPONSE_H
#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <sys/types.h>  // off_t

// HttpResponse: HTTP response builder
// Supports status code, headers, body and keep-alive control
class HttpResponse {
public:
    enum HttpStatusCode {
        kUnknown = 0,
        k200Ok = 200,
        k206PartialContent = 206,
        k301MovedPermanently = 301,
        k302Found = 302,
        k304NotModified = 304,
        k400BadRequest = 400,
        k403Forbidden = 403,
        k404NotFound = 404,
        k405MethodNotAllowed = 405,
        k413PayloadTooLarge = 413,
        k500InternalServerError = 500,
        k503ServiceUnavailable = 503,
    };

    explicit HttpResponse(bool closeConnection = false)
        : statusCode_(kUnknown),
          closeConnection_(closeConnection) {}

    void setStatusCode(HttpStatusCode code) { statusCode_ = code; }
    HttpStatusCode statusCode() const { return statusCode_; }
    void setStatusMessage(const std::string& msg) { statusMessage_ = msg; }

    void setCloseConnection(bool on) { closeConnection_ = on; }
    bool closeConnection() const { return closeConnection_; }

    void setContentType(const std::string& contentType) {
        addHeader("Content-Type", contentType);
    }

    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    bool hasHeader(const std::string& key) const {
        return headers_.find(key) != headers_.end();
    }

    void setBody(const std::string& body) { body_ = body; }
    void setBody(std::string&& body) { body_ = std::move(body); }

    // --- zero-copy file body (sendfile path) ---
    // fd ownership moves to the sender: serialize() only produces headers (Content-Length=fileSize);
    // the body is pushed by TcpConnection::sendFile via sendfile(2), data never enters user space
    void setFileBody(int fd, off_t offset, size_t size) {
        fileFd_ = fd;
        fileOffset_ = offset;
        fileSize_ = size;
    }
    bool hasFileBody() const { return fileFd_ >= 0; }
    int takeFileFd() { int fd = fileFd_; fileFd_ = -1; return fd; }  // transfer ownership out
    off_t fileOffset() const { return fileOffset_; }
    size_t fileSize() const { return fileSize_; }

    // serialize the response into a string (for send)
    std::string serialize() const;

    // convenience builders
    static HttpResponse ok(const std::string& body, const std::string& contentType = "text/html");
    static HttpResponse notFound(const std::string& body = "");
    static HttpResponse badRequest(const std::string& body = "");
    static HttpResponse forbidden(const std::string& body = "");
    static HttpResponse serverError(const std::string& body = "");
    static HttpResponse methodNotAllowed(const std::string& body = "");

private:
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    bool closeConnection_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;

    int fileFd_ = -1;       // sendfile path: file descriptor (-1 = normal body)
    off_t fileOffset_ = 0;
    size_t fileSize_ = 0;
};
#endif // AETHER_HTTP_HTTPRESPONSE_H
