#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <vector>
#include <atomic>
#include <fstream>

/// AsyncLogger: Async double-buffered logging system
///
/// Design:
/// - Frontend (any thread): LOG_XXX macros -> format into FrontBuffer -> wake up backend thread
/// - Backend (dedicated thread): every flushInterval seconds or when FrontBuffer is full,
///   swap Front/Back Buffer, write Back to file
/// - Double buffering ensures frontend writes don't block, backend file writes don't affect frontend
///
/// Rotation strategy:
/// - By size: rotate when file exceeds rollSize_
/// - By date: auto-rotate to new file at midnight
/// - Keep N old log files, auto-delete when exceeded
class AsyncLogger {
public:
    static const int kBufferSize = 4 * 1024 * 1024;  // 4MB

    class Buffer {
    public:
        Buffer() : data_(kBufferSize), used_(0) {}

        void append(const char* buf, size_t len) {
            if (static_cast<size_t>(avail()) > len) {
                memcpy(data_.data() + used_, buf, len);
                used_ += static_cast<int>(len);
            }
        }

        const char* data() const { return data_.data(); }
        int length() const { return used_; }
        int avail() const { return kBufferSize - used_; }
        void reset() { used_ = 0; }
        bool empty() const { return used_ == 0; }

    private:
        std::vector<char> data_;
        int used_;
    };

    static AsyncLogger& instance();

    void start();
    void stop();

    void append(const char* logline, int len);

    void setLogFile(const std::string& path) { logFile_ = path; }
    void setFlushInterval(int seconds) { flushInterval_ = seconds; }
    void setRollSize(size_t size) { rollSize_ = size; }
    void setKeepFiles(int n) { keepFiles_ = n; }

private:
    AsyncLogger();
    ~AsyncLogger();

    void threadFunc();

    std::string getLogFileName(const struct tm& tm_now);
    void rollFile();
    void cleanOldFiles();
    bool isSameDay(const struct tm& a, const struct tm& b);

    std::string logFile_;
    int flushInterval_;
    size_t rollSize_;
    int keepFiles_;     // Keep N old log files
    size_t writtenBytes_;
    int lastDay_;       // Last roll date (tm_mday) for day-rollover detection

    std::unique_ptr<Buffer> frontBuf_;
    std::unique_ptr<Buffer> backBuf_;
    std::vector<std::unique_ptr<Buffer>> buffersToWrite_;

    std::mutex mtx_;
    std::condition_variable cond_;
    std::thread thread_;
    std::atomic_bool running_;

    std::ofstream fileStream_;
};
