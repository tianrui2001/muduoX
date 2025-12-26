#include "InetAddress.h"

#include <string>
#include <arpa/inet.h>
#include <cstring>

// h = host (主机)
// n = network (网络)
// to = to (到)
// s = short (短整型, 16位)
InetAddress::InetAddress(uint16_t port = 0, std::string ip ){
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());
}

// inet_pton 和 inet_ntop 处理 IP 地址（32位或128位）的字节序转换
std::string InetAddress::toIp() const{
    char buf[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return std::string(buf);
}

uint16_t InetAddress::toPort() const{
    return ntohs(addr_.sin_port);
}

std::string InetAddress::toIpPort() const{
    std::string ip = toIp();
    uint16_t port = toPort();

    return ip + ":" + std::to_string(port);
}