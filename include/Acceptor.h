#pragma once

#include <functional>

#include "Socket.h"
#include "Channel.h"
#include "Nocopyable.h"

class EventLoop;
class InetAddress;

class Acceptor : nocopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress &)>;


    Acceptor(EventLoop *loop, const InetAddress &listenAddr, bool reuseport);
    ~Acceptor();

    // 监听本地端口
    void listen();

    void setNewConnectionCallback(const NewConnectionCallback &cb) {
        newConnectionCallback_  = cb;
    }

    bool isListening() const { return listening_; }
private:
    void handleRead();  // 处理新用户连接

    EventLoop *loop_;   // mainLoop事件循环
    Socket acceptSocket_;   //专门用于接收新连接的socket
    Channel acceptChannel_; //专门用于监听新连接的channel

    NewConnectionCallback newConnectionCallback_;   //新连接的回调函数
    bool listening_;    //是否在监听
};
