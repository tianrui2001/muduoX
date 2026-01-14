#pragma once

#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <unordered_map>

#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "Nocopyable.h"
#include "EventLoopThreadPool.h"
#include "Callbacks.h"
#include "TcpConnection.h"
#include "Buffer.h"
#include "TimeStamp.h"

class TcpServer : nocopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;
    using ConnectionMap = std::unordered_map<std::string, TcpConnectionPtr>;
    enum Option
    {
        kNoReusePort,
        kReusePort,
    };

    TcpServer(EventLoop *loop,
              const InetAddress &listenAddr,
              const std::string &nameArg,
              Option option = kNoReusePort);

    ~TcpServer();
    void setThreadInitCallback(const ThreadInitCallback &cb)   { threadInitCallback_ = cb; }
    void setConnectionCallback(const ConnectionCallback &cb)   { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb)         { messageCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }

    void setThreadNum(int numThreads);  //  设置底层subLoop的数量
    void start();   // 启动服务器
private:
    void newConnection(int sockfd, const InetAddress &peerAddr); // 有新连接到来时的回调
    void removeConnection(const TcpConnectionPtr &conn); // 连接关闭时的回调
    void removeConnectionInLoop(const TcpConnectionPtr &conn);  // 连接关闭时的回调，运行在loop线程中

    EventLoop *loop_;  // 用户定义的mainLoop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_; // 运行在mainLoop，负责监听新连接
    std::shared_ptr<EventLoopThreadPool> threadPool_; // one loop per thread

    ConnectionCallback connectionCallback_;       // 新连接回调
    MessageCallback messageCallback_;             // 消息回调
    WriteCompleteCallback writeCompleteCallback_; // 写完成回调
    ThreadInitCallback threadInitCallback_;       // 线程初始化回调
    
    std::atomic_int started_;   // 标识TcpServer是否已经启动
    int nextConnId_;            // 下一个连接的ID
    ConnectionMap connections_; // 存储所有连接的map
};