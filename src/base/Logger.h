#pragma once
#include <string>
#include <cstdio>
#include <cstdarg>
#include <mutex>

// 日志级别
enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// Logger：日志接口
// 默认同步写stderr，可切换为异步模式（写入文件）
class Logger {
public:
    static Logger& instance();
    void setLevel(LogLevel level);
    void log(LogLevel level, const char* file, int line, const char* fmt, ...);

    // 启用异步日志（写文件）
    void enableAsync(const std::string& logFile = "./aether.log");

    // Get current log level
    LogLevel level() const { return level_; }

private:
    Logger();
    LogLevel level_;
    std::mutex mtx_;
    bool asyncMode_;
    const char* levelStr(LogLevel level);
};

#define LOG_DEBUG(...) Logger::instance().log(LogLevel::DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  Logger::instance().log(LogLevel::INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  Logger::instance().log(LogLevel::WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().log(LogLevel::ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(...) Logger::instance().log(LogLevel::FATAL, __FILE__, __LINE__, __VA_ARGS__)
