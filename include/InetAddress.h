#pragma once

#include <netinet/in.h>
#include <string>


class InetAddress {
public:
    explicit InetAddress(const sockaddr_in &addr)
        : addr_(addr) {}

    InetAddress(uint16_t port = 0, std::string ip = "127.0.0.1");
    std::string toIp() const;
    uint16_t toPort() const;
    std::string toIpPort() const;

    const sockaddr_in* getSockAddr() const { return &addr_;}
    void setSockAddr(const sockaddr_in &addr) { addr_ = addr;}

private:
    sockaddr_in addr_;
};