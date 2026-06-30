#include "HttpResponse.h"
#include <sstream>

// ─── Serialize response to wire format ───
std::string HttpResponse::serialize() const {
    std::ostringstream oss;

    // Status line: HTTP/1.1 <code> <message>\r\n
    oss << "HTTP/1.1 " << statusCode_ << " " << statusMessage_ << "\r\n";

    // Headers
    for (auto& [key, value] : headers_) {
        oss << key << ": " << value << "\r\n";
    }

    // Ensure Content-Length is set
    if (!hasHeader("Content-Length")) {
        oss << "Content-Length: " << body_.size() << "\r\n";
    }

    // Connection control
    if (!hasHeader("Connection")) {
        if (closeConnection_) {
            oss << "Connection: close\r\n";
        } else {
            oss << "Connection: keep-alive\r\n";
        }
    }

    // Server header
    if (!hasHeader("Server")) {
        oss << "Server: Aether\r\n";
    }

    // End of headers
    oss << "\r\n";

    // Body (caller decides whether to set it; HEAD requests should not set body)
    if (!body_.empty()) {
        oss << body_;
    }

    return oss.str();
}

// ─── Quick builders ───

HttpResponse HttpResponse::ok(const std::string& body, const std::string& contentType) {
    HttpResponse resp(false);
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
    resp.setContentType("text/html");
    resp.setBody(body.empty() ? "<html><body><h1>404 Not Found</h1></body></html>" : body);
    return resp;
}

HttpResponse HttpResponse::badRequest(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k400BadRequest);
    resp.setStatusMessage("Bad Request");
    resp.setContentType("text/html");
    resp.setBody(body.empty() ? "<html><body><h1>400 Bad Request</h1></body></html>" : body);
    return resp;
}

HttpResponse HttpResponse::forbidden(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k403Forbidden);
    resp.setStatusMessage("Forbidden");
    resp.setContentType("text/html");
    resp.setBody(body.empty() ? "<html><body><h1>403 Forbidden</h1></body></html>" : body);
    return resp;
}

HttpResponse HttpResponse::serverError(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k500InternalServerError);
    resp.setStatusMessage("Internal Server Error");
    resp.setContentType("text/html");
    resp.setBody(body.empty() ? "<html><body><h1>500 Internal Server Error</h1></body></html>" : body);
    return resp;
}

HttpResponse HttpResponse::methodNotAllowed(const std::string& body) {
    HttpResponse resp(true);
    resp.setStatusCode(k405MethodNotAllowed);
    resp.setStatusMessage("Method Not Allowed");
    resp.setContentType("text/html");
    resp.setBody(body.empty() ? "<html><body><h1>405 Method Not Allowed</h1></body></html>" : body);
    return resp;
}
