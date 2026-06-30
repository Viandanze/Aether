#pragma once
#include <vector>
#include <sys/epoll.h>
#include <unistd.h>
#include "base/noncopyable.h"

class Channel;

// Epoller: supports LT/ET mode
class Epoller : noncopyable {
public:
    Epoller();
    ~Epoller();

    // Event operations
    void updateChannel(Channel* channel);   // Add/modify
    void removeChannel(Channel* channel);   // Delete

    // Block wait for events, return ready Channel list
    using ChannelList = std::vector<Channel*>;
    int poll(int timeoutMs, ChannelList* activeChannels);

private:
    static const int kInitEventListSize = 16;

    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;
    void update(int operation, Channel* channel);

    int epollFd_;
    std::vector<struct epoll_event> events_;
};
