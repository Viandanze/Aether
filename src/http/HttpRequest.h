#pragma once
#include <string>
#include <unordered_map>
#include <algorithm>

// HttpRequest：HTTP请求的解析结果
class HttpRequest {
public:
    enum Method { kInvalid, kGet, kPost, kHead, kPut, kDelete };
    enum Version { kUnknown, kHttp10, kHttp11 };

    HttpRequest()
        : method_(kInvalid),
          version_(kUnknown) {}

    // --- Method ---
    void setMethod(Method m) { method_ = m; }
    Method method() const { return method_; }
    const char* methodString() const {
        const char* names[] = { "INVALID", "GET", "POST", "HEAD", "PUT", "DELETE" };
        return names[method_];
    }

    // --- Version ---
    void setVersion(Version v) { version_ = v; }
    Version version() const { return version_; }

    // --- Path ---
    void setPath(const std::string& p) { path_ = p; }
    const std::string& path() const { return path_; }

    // --- Query ---
    void setQuery(const std::string& q) { query_ = q; }
    const std::string& query() const { return query_; }

    // --- Headers ---
    void addHeader(const std::string& key, const std::string& value) {
        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        headers_[lowerKey] = value;
    }
    std::string getHeader(const std::string& key) const {
        std::string lowerKey = key;
        std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
        auto it = headers_.find(lowerKey);
        return it != headers_.end() ? it->second : std::string();
    }
    const std::unordered_map<std::string, std::string>& headers() const { return headers_; }

    // --- Body ---
    void setBody(const std::string& b) { body_ = b; }
    void setBody(std::string&& b) { body_ = std::move(b); }
    void appendBody(const std::string& data) { body_.append(data); }
    const std::string& body() const { return body_; }

    // --- Content-Length ---
    size_t contentLength() const {
        auto it = headers_.find("content-length");
        if (it != headers_.end()) {
            try {
                return static_cast<size_t>(std::stoul(it->second));
            } catch (...) {
                return 0;
            }
        }
        return 0;
    }

    // --- 便捷方法 ---
    bool isKeepAlive() const {
        if (version_ == kHttp10) {
            return getHeader("Connection") == "keep-alive";
        }
        // HTTP/1.1 默认keep-alive
        return getHeader("Connection") != "close";
    }

    // --- Debug ---
    std::string dump() const;

private:
    Method method_;
    Version version_;
    std::string path_;
    std::string query_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};
