#pragma once
#include <string>
#include <unordered_map>
#include <functional>

// HttpResponse: build HTTP response
// Supports status code, headers, body, Keep-Alive control
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

    // Serialize response to string (for send)
    std::string serialize() const;

    // Quick builders
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
};
