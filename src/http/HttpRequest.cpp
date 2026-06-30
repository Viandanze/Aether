#include "HttpRequest.h"
#include <sstream>

std::string HttpRequest::dump() const {
    std::ostringstream oss;
    oss << methodString() << " " << path_;
    if (!query_.empty()) oss << "?" << query_;
    oss << " HTTP/" << (version_ == kHttp11 ? "1.1" : "1.0") << "\r\n";
    for (auto& [k, v] : headers_) {
        oss << k << ": " << v << "\r\n";
    }
    oss << "\r\n";
    if (!body_.empty()) {
        oss << body_;
    }
    return oss.str();
}
