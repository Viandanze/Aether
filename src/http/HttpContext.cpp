#include "HttpContext.h"
#include "net/Buffer.h"
#include "base/Logger.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>

static const char kCRLF[] = "\r\n";

bool HttpContext::parseRequest(Buffer* buf) {
    size_t consumed = 0;
    const char* bufData = buf->peek();
    size_t len = buf->readableBytes();
    size_t pos = 0;

    while (pos < len) {
        if (state_ == kExpectRequestLine || state_ == kExpectHeaders) {
            // Find \r\n (use std::search instead of GNU extension memmem)
            const char* searchStart = bufData + pos;
            size_t searchLen = len - pos;
            const char* crlf = std::search(searchStart, searchStart + searchLen,
                                           kCRLF, kCRLF + 2);

            if (crlf == nullptr) {
                break;  // 不完整行，等待更多数据
            }

            size_t lineLen = static_cast<size_t>(crlf - (bufData + pos));
            std::string_view line(bufData + pos, lineLen);
            pos += lineLen + 2;

            if (state_ == kExpectRequestLine) {
                if (!processRequestLine(line)) {
                    LOG_ERROR("HttpContext: bad request line: %.*s",
                              static_cast<int>(lineLen), line.data());
                    return false;
                }
                state_ = kExpectHeaders;
            } else if (state_ == kExpectHeaders) {
                if (line.empty()) {
                    std::string transferEncoding = request_.getHeader("Transfer-Encoding");
                    if (transferEncoding.find("chunked") != std::string::npos) {
                        state_ = kExpectChunkSize;
                    } else {
                        size_t bodyLen = request_.contentLength();
                        if (bodyLen > 0) {
                            state_ = kExpectBody;
                        } else {
                            state_ = kGotComplete;
                        }
                    }
                } else {
                    auto colon = std::find(line.begin(), line.end(), ':');
                    if (colon == line.end()) {
                        LOG_ERROR("HttpContext: bad header: %.*s",
                                  static_cast<int>(lineLen), line.data());
                        return false;
                    }
                    std::string key(line.begin(), colon);
                    auto valStart = colon + 1;
                    if (valStart != line.end() && *valStart == ' ') {
                        ++valStart;
                    }
                    std::string value(valStart, line.end());
                    request_.addHeader(key, value);
                }
            }
        } else if (state_ == kExpectBody) {
            size_t bodyLen = request_.contentLength();
            size_t remaining = len - pos;

            if (remaining >= bodyLen) {
                request_.setBody(std::string(bufData + pos, bodyLen));
                pos += bodyLen;
                state_ = kGotComplete;
            } else {
                break;
            }
        } else if (state_ == kExpectChunkSize) {
            const char* searchStart = bufData + pos;
            size_t searchLen = len - pos;
            const char* crlf = std::search(searchStart, searchStart + searchLen,
                                           kCRLF, kCRLF + 2);

            if (crlf == searchStart + searchLen) {
                crlf = nullptr;
            }

            if (crlf == nullptr) {
                break;
            }

            size_t lineLen = static_cast<size_t>(crlf - (bufData + pos));
            std::string_view line(bufData + pos, lineLen);
            pos += lineLen + 2;

            if (!processChunkSize(line)) {
                LOG_ERROR("HttpContext: bad chunk size: %.*s",
                          static_cast<int>(lineLen), line.data());
                return false;
            }

            if (chunkSize_ == 0) {
                state_ = kExpectChunkFinal;
            } else {
                state_ = kExpectChunkData;
            }
        } else if (state_ == kExpectChunkData) {
            size_t remaining = len - pos;
            size_t needed = chunkSize_ + 2;

            if (remaining >= needed) {
                request_.appendBody(std::string(bufData + pos, chunkSize_));
                pos += chunkSize_ + 2;
                state_ = kExpectChunkSize;
            } else {
                break;
            }
        } else if (state_ == kExpectChunkFinal) {
            const char* searchStart = bufData + pos;
            size_t searchLen = len - pos;
            const char* crlf = std::search(searchStart, searchStart + searchLen,
                                           kCRLF, kCRLF + 2);

            if (crlf == searchStart + searchLen) {
                crlf = nullptr;
            }

            if (crlf == nullptr) {
                break;
            }

            size_t lineLen = static_cast<size_t>(crlf - (bufData + pos));
            pos += lineLen + 2;

            if (lineLen == 0) {
                state_ = kGotComplete;
            }
        } else {
            break;
        }
    }

    consumed = pos;
    if (consumed > 0) {
        buf->retrieve(consumed);
    }
    return true;
}

bool HttpContext::processRequestLine(std::string_view line) {
    auto sp1 = std::find(line.begin(), line.end(), ' ');
    if (sp1 == line.end()) return false;

    auto sp2 = std::find(sp1 + 1, line.end(), ' ');
    if (sp2 == line.end()) return false;

    std::string_view methodStr(line.begin(), sp1);
    HttpRequest::Method method = HttpRequest::kInvalid;
    if (methodStr == "GET")          method = HttpRequest::kGet;
    else if (methodStr == "POST")    method = HttpRequest::kPost;
    else if (methodStr == "HEAD")    method = HttpRequest::kHead;
    else if (methodStr == "PUT")     method = HttpRequest::kPut;
    else if (methodStr == "DELETE")  method = HttpRequest::kDelete;

    if (method == HttpRequest::kInvalid) {
        LOG_ERROR("HttpContext: unknown method: %.*s",
                  static_cast<int>(methodStr.size()), methodStr.data());
        return false;
    }
    request_.setMethod(method);

    std::string_view url(sp1 + 1, sp2);
    auto questionMark = std::find(url.begin(), url.end(), '?');
    if (questionMark != url.end()) {
        request_.setPath(std::string(url.begin(), questionMark));
        request_.setQuery(std::string(questionMark + 1, url.end()));
    } else {
        request_.setPath(std::string(url));
    }

    std::string_view version(sp2 + 1, line.end());
    if (version == "HTTP/1.1") {
        request_.setVersion(HttpRequest::kHttp11);
    } else if (version == "HTTP/1.0") {
        request_.setVersion(HttpRequest::kHttp10);
    } else {
        LOG_ERROR("HttpContext: unknown version: %.*s",
                  static_cast<int>(version.size()), version.data());
        return false;
    }

    return true;
}

bool HttpContext::processChunkSize(std::string_view line) {
    // "1a\r\n" 或 "0\r\n"
    // chunk-size可能带扩展，用分号分隔，只取数字部分
    auto semi = std::find(line.begin(), line.end(), ';');
    std::string_view sizeStr = (semi != line.end())
        ? std::string_view(line.begin(), semi)
        : line;

    try {
        chunkSize_ = static_cast<size_t>(std::stoul(std::string(sizeStr), nullptr, 16));
    } catch (...) {
        return false;
    }
    return true;
}
