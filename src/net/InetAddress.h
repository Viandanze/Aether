#pragma once
#include <string>
#include <netinet/in.h>

// IP address + port wrapper
class InetAddress {
public:
    explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false);
    InetAddress(const std::string& ip, uint16_t port);

    const struct sockaddr_in& getSockAddr() const { return addr_; }
    void setSockAddr(const struct sockaddr_in& addr) { addr_ = addr; }

    std::string toIp()   const;
    std::string toIpPort() const;
    uint16_t toPort() const;

private:
    struct sockaddr_in addr_;
};
