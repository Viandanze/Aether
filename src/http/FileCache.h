#ifndef AETHER_HTTP_FILECACHE_H
#define AETHER_HTTP_FILECACHE_H
#pragma once
#include <string>
#include <mutex>
#include <list>
#include <unordered_map>
#include <fstream>
#include <fcntl.h>
#include <sys/stat.h>
#include "base/noncopyable.h"
#include "http/HttpRequest.h"
#include "http/HttpResponse.h"

/// FileCache: in-memory LRU cache for static files
///
/// Design notes:
/// 1. Hit validation: compare mtime + size; entries invalidate automatically on file change (no TTL needed)
/// 2. LRU eviction: evict least recently used entries when total bytes exceed the limit
/// 3. Large-file bypass: files above maxFileBytes skip the cache and hit disk every time
/// 4. Conditional requests: ETag(mtime+size) + If-None-Match -> 304, saves bandwidth
/// 5. Thread safety: mutex-protected for concurrent IO threads; stat() runs outside the lock
///
/// Benchmark context: without the cache every request pays stat+open+read+close,
/// which dominates throughput on slow filesystems (measured 170 QPS -> tens of thousands with the cache)
class FileCache : noncopyable {
public:
    static FileCache& instance() {
        static FileCache cache;
        return cache;
    }

    /// serveDir: static file root (the -d option in main)
    /// maxTotalBytes: total cache budget; maxFileBytes: per-file cache limit
    void init(const std::string& serveDir,
              size_t maxTotalBytes = 64 * 1024 * 1024,
              size_t maxFileBytes  = 4 * 1024 * 1024) {
        serveDir_       = serveDir;
        maxTotalBytes_  = maxTotalBytes;
        maxFileBytes_   = maxFileBytes;
    }

    /// Handle a static file request (GET/HEAD). Always fills resp completely (200/304/404)
    void serve(const HttpRequest& req, HttpResponse* resp) {
        const std::string& path = req.path();
        const std::string filePath =
            serveDir_ + (path == "/" ? "/index.html" : path);

        struct stat st;
        if (::stat(filePath.c_str(), &st) != 0 || S_ISDIR(st.st_mode)) {
            *resp = HttpResponse::notFound();
            return;
        }

        const std::string etag = makeEtag(st);

        // conditional request: If-None-Match hit -> 304 (no body)
        if (req.getHeader("If-None-Match") == etag) {
            resp->setStatusCode(HttpResponse::k304NotModified);
            resp->setStatusMessage("Not Modified");
            resp->addHeader("ETag", etag);
            return;
        }

        // large-file bypass: not cached. GET goes through zero-copy sendfile(2) (fd handed to the sender),
        // data never enters user space, saving the read-into-memory + write double copy
        if (static_cast<size_t>(st.st_size) > maxFileBytes_) {
            serveLargeFile(filePath, st, req, resp, etag);
            return;
        }

        std::string body;
        if (!lookupOrLoad(filePath, st, body)) {
            *resp = HttpResponse::notFound();
            return;
        }

        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType(mimeTypeOf(filePath));
        resp->addHeader("ETag", etag);
        if (req.method() == HttpRequest::kGet) {
            resp->setBody(std::move(body));   // HEAD carries no body
        } else {
            // RFC 7230: HEAD's Content-Length must equal what GET would return
            resp->addHeader("Content-Length", std::to_string(body.size()));
        }
    }

    size_t cachedFiles() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return map_.size();
    }
    size_t cachedBytes() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return totalBytes_;
    }

private:
    struct Entry {
        std::string body;
        std::string mime;
        time_t mtime;
        off_t   size;
        std::list<std::string>::iterator lruIt;   // our own position in lru_
    };

    FileCache() = default;

    static std::string makeEtag(const struct stat& st) {
        return "\"" + std::to_string(static_cast<long long>(st.st_mtime)) +
               "-" + std::to_string(static_cast<long long>(st.st_size)) + "\"";
    }

    static std::string mimeTypeOf(const std::string& path) {
        auto dot = path.rfind('.');
        std::string ext = (dot == std::string::npos) ? "" : path.substr(dot);
        if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
        if (ext == ".css")  return "text/css";
        if (ext == ".js")   return "application/javascript";
        if (ext == ".json") return "application/json";
        if (ext == ".png")  return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif")  return "image/gif";
        if (ext == ".svg")  return "image/svg+xml";
        if (ext == ".ico")  return "image/x-icon";
        if (ext == ".txt")  return "text/plain; charset=utf-8";
        if (ext == ".xml")  return "text/xml";
        if (ext == ".pdf")  return "application/pdf";
        if (ext == ".woff" || ext == ".woff2") return "font/woff";
        if (ext == ".ttf")  return "font/ttf";
        return "application/octet-stream";
    }

    static bool readFile(const std::string& filePath, std::string& out) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return false;
        out.assign(std::istreambuf_iterator<char>(file),
                   std::istreambuf_iterator<char>());
        return true;
    }

    /// Large-file bypass: open the fd and hand it to the sender for zero-copy sendfile(2).
    /// HEAD returns headers only (real Content-Length included, no file open)
    void serveLargeFile(const std::string& filePath, const struct stat& st,
                        const HttpRequest& req, HttpResponse* resp,
                        const std::string& etag) {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType(mimeTypeOf(filePath));
        resp->addHeader("ETag", etag);
        if (req.method() != HttpRequest::kGet) {
            // HEAD: report the real size, transfer no body, no need to open the file
            resp->addHeader("Content-Length",
                            std::to_string(static_cast<long long>(st.st_size)));
            return;
        }
        int fd = ::open(filePath.c_str(), O_RDONLY);
        if (fd < 0) {
            // stat succeeded but open failed (permissions changed, etc.)
            *resp = HttpResponse::forbidden();
            return;
        }
        // fd ownership transfer: HttpResponse holds it briefly, HttpServer takes it and passes it to TcpConnection::sendFile
        resp->setFileBody(fd, 0, static_cast<size_t>(st.st_size));
    }

    /// On hit, return a copy of the body; on miss, read from disk and cache per policy
    bool lookupOrLoad(const std::string& filePath, const struct stat& st,
                      std::string& body) {
        const bool cacheable =
            static_cast<size_t>(st.st_size) <= maxFileBytes_;

        // -- fast path: cache lookup under the lock (a hit only copies the string) --
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = map_.find(filePath);
            if (it != map_.end()) {
                Entry& e = it->second;
                if (e.mtime == st.st_mtime && e.size == st.st_size) {
                    lru_.splice(lru_.begin(), lru_, e.lruIt);   // touch
                    body = e.body;
                    return true;
                }
                evict(it);   // file changed, the old entry is dead
            }
        }

        // -- slow path: read from disk outside the lock --
        if (!readFile(filePath, body)) return false;

        if (cacheable) {
            std::lock_guard<std::mutex> lk(mtx_);
            auto [it2, inserted] = map_.emplace(filePath, Entry{});
            if (inserted) {
                Entry& e = it2->second;
                e.body  = body;
                e.mtime = st.st_mtime;
                e.size  = st.st_size;
                lru_.push_front(filePath);
                e.lruIt = lru_.begin();
                totalBytes_ += body.size();
                evictUntilLimit();
            }
            // under concurrency another thread may have just inserted this file; simply skip inserting
        }
        return true;
    }

    void evict(std::unordered_map<std::string, Entry>::iterator it) {
        totalBytes_ -= it->second.body.size();
        lru_.erase(it->second.lruIt);
        map_.erase(it);
    }

    void evictUntilLimit() {
        while (totalBytes_ > maxTotalBytes_ && !lru_.empty()) {
            auto it = map_.find(lru_.back());
            if (it != map_.end()) evict(it);
            else lru_.pop_back();
        }
    }

    std::string serveDir_ = "./www";
    size_t maxTotalBytes_ = 64 * 1024 * 1024;
    size_t maxFileBytes_  = 4 * 1024 * 1024;

    mutable std::mutex mtx_;
    std::unordered_map<std::string, Entry> map_;
    std::list<std::string> lru_;          // front = most recently used
    size_t totalBytes_ = 0;
};
#endif // AETHER_HTTP_FILECACHE_H
