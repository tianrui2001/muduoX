#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>

Socket::~Socket(){
    ::close(sockfd_);
}

void Socket::bindAddress(const InetAddress &addr){
    int ret = ::bind(sockfd_, (sockaddr*)addr.getSockAddr(), sizeof(sockaddr_in));
    if(ret){
        LOG_FATAL << "Bind sockfd:" << sockfd_ << " fail\n";
    }
}

void Socket::listen(){
    int ret = ::listen(sockfd_, 1024);
    if(ret){
        LOG_FATAL << "Listen sockfd:" << sockfd_ << " fail\n";
    }
}

// 从监听队列中取出一个已完成的客户端连接，并为其创建一个新的、专用的socket文件描述符
int Socket::accept(InetAddress* peeraddr){
    sockaddr_in addr;
    bzero(&addr, sizeof addr);
    socklen_t addrlen = sizeof(addr);

    // 应对返回的connfd设置非阻塞: Reactor模型 one loop per thread, poller + non-blocking IO
    int connfd = ::accept4(sockfd_, (sockaddr*)&addr, &addrlen, SOCK_CLOEXEC | SOCK_NONBLOCK);
    if(connfd >= 0){
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

// 只关闭了写方向，程序仍然可以从这个socket读取数据，直到收到对方的FIN包
void Socket::shutdownWrite(){

    int ret = ::shutdown(sockfd_, SHUT_WR);
    if(ret < 0){
        LOG_ERROR << "ShutdownWrite sockfd:" << sockfd_ << " fail\n";
    }
}

void Socket::setTcpNoDelay(bool on){
    // TCP_NODELAY 用于禁用 Nagle 算法。
    // Nagle 算法用于减少网络上传输的小数据包数量。
    // 将 TCP_NODELAY 设置为 1 可以禁用该算法，允许小数据包立即发送,以减少延迟。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof optval);
}

/**
 * 当一个服务器关闭后，它监听的端口会进入一个 TIME_WAIT 状态，持续几十秒到几分钟。
 * 在这期间，你无法立即重启服务器并绑定到同一个端口，bind会失败。
 * setReuseAddr(true) 允许你的新进程立即绑定到这个处于 TIME_WAIT 状态的端口，
 * 极大地方便了服务器的快速重启和开发调试。
 */
void Socket::setReuseAddr(bool on){
    // SO_REUSEADDR 允许一个套接字强制绑定到一个已被其他套接字使用的端口。
    // 这对于需要重启并绑定到相同端口的服务器应用程序非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);
}

void Socket::setReusePort(bool on){
    // SO_REUSEPORT 允许同一主机上的多个套接字绑定到相同的端口号。
    // 这对于在多个线程或进程之间负载均衡传入连接非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof optval);
}   

void Socket::setKeepAlive(bool on){
    // SO_KEEPALIVE 启用在已连接的套接字上定期传输消息。
    // 如果另一端没有响应，则认为连接已断开并关闭。
    // 这对于检测网络中失效的对等方非常有用。
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof optval);
}