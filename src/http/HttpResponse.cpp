#include "HttpResponse.h"
#include <cstdio>
#include <sstream>

// server identity
static const char* kServerName = "Aether/0.5";

std::string HttpResponse::serialize() const {
    std::ostringstream oss;

    // status line: HTTP/1.1 200 OK\r\n
    char buf[64];
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
    oss << buf << statusMessage_ << "\r\n";

    // Server header
    oss << "Server: " << kServerName << "\r\n";

    // Content-Length (added automatically)
    // file body: length comes from fileSize_; the body is filled in by sendfile(2) in the sender, never materialized here
    if (hasFileBody()) {
        if (!headers_.count("Content-Length")) {
            oss << "Content-Length: " << fileSize_ << "\r\n";
        }
    } else if (!body_.empty()) {
        bool hasContentLength = headers_.find("Content-Length") != headers_.end();
        if (!hasContentLength) {
            oss << "Content-Length: " << body_.size() << "\r\n";
        }
    } else {
        // even an empty body sends Content-Length: 0
        if (!headers_.count("Content-Length") && !headers_.count("Transfer-Encoding")) {
            oss << "Content-Length: 0\r\n";
        }
    }

    // Connection
    if (closeConnection_) {
        oss << "Connection: close\r\n";
    } else {
        oss << "Connection: keep-alive\r\n";
    }

    // Date header
    time_t now = time(nullptr);
    struct tm tm_buf;
    gmtime_r(&now, &tm_buf);
    char dateBuf[64];
    strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y %H:%M:%S GMT", &tm_buf);
    oss << "Date: " << dateBuf << "\r\n";

    // custom headers
    for (auto& [key, value] : headers_) {
        oss << key << ": " << value << "\r\n";
    }

    // blank line
    oss << "\r\n";

    // Body
    if (!body_.empty()) {
        oss << body_;
    }

    return oss.str();
}

// --- convenience builders ---

HttpResponse HttpResponse::ok(const std::string& body, const std::string& contentType) {
    HttpResponse resp;
    resp.setStatusCode(k200Ok);
    resp.setStatusMessage("OK");
    resp.setContentType(contentType);
    resp.setBody(body);
    return resp;
}

HttpResponse HttpResponse::notFound(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k404NotFound);
    resp.setStatusMessage("Not Found");
    resp.setContentType("text/html; charset=utf-8");
    if (body.empty()) {
        resp.setBody("<html><head><title>404</title></head>"
                     "<body><h1>404 Not Found</h1>"
                     "<p>The requested resource was not found on this server.</p>"
                     "<hr><p><i>Aether/0.5</i></p>"
                     "</body></html>");
    } else {
        resp.setBody(body);
    }
    return resp;
}

HttpResponse HttpResponse::badRequest(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k400BadRequest);
    resp.setStatusMessage("Bad Request");
    resp.setContentType("text/html; charset=utf-8");
    if (body.empty()) {
        resp.setBody("<html><head><title>400</title></head>"
                     "<body><h1>400 Bad Request</h1>"
                     "<p>The server could not understand the request.</p>"
                     "<hr><p><i>Aether/0.5</i></p>"
                     "</body></html>");
    } else {
        resp.setBody(body);
    }
    return resp;
}

HttpResponse HttpResponse::forbidden(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k403Forbidden);
    resp.setStatusMessage("Forbidden");
    resp.setContentType("text/html; charset=utf-8");
    if (body.empty()) {
        resp.setBody("<html><head><title>403</title></head>"
                     "<body><h1>403 Forbidden</h1>"
                     "<p>You don't have permission to access this resource.</p>"
                     "<hr><p><i>Aether/0.5</i></p>"
                     "</body></html>");
    } else {
        resp.setBody(body);
    }
    return resp;
}

HttpResponse HttpResponse::serverError(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k500InternalServerError);
    resp.setStatusMessage("Internal Server Error");
    resp.setContentType("text/html; charset=utf-8");
    if (body.empty()) {
        resp.setBody("<html><head><title>500</title></head>"
                     "<body><h1>500 Internal Server Error</h1>"
                     "<p>An internal server error occurred.</p>"
                     "<hr><p><i>Aether/0.5</i></p>"
                     "</body></html>");
    } else {
        resp.setBody(body);
    }
    return resp;
}

HttpResponse HttpResponse::methodNotAllowed(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k405MethodNotAllowed);
    resp.setStatusMessage("Method Not Allowed");
    resp.setContentType("text/html; charset=utf-8");
    resp.addHeader("Allow", "GET, HEAD, POST");
    if (body.empty()) {
        resp.setBody("<html><head><title>405</title></head>"
                     "<body><h1>405 Method Not Allowed</h1>"
                     "<p>The request method is not supported.</p>"
                     "<hr><p><i>Aether/0.5</i></p>"
                     "</body></html>");
    } else {
        resp.setBody(body);
    }
    return resp;
}
