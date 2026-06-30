#pragma once
// Non-copyable base class, similar to muduo's noncopyable
class noncopyable {
public:
    noncopyable() = default;
    ~noncopyable() = default;
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};
