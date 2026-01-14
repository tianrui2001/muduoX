#pragma once

#include <memory>
#include <string>
#include <atomic>

#include "Nocopyable.h"
#include "InetAddress.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "TimeStamp.h"

class Channel;
class EventLoop;
class Socket;

/**
 * TcpServer => Acceptor => 有一个新用户连接，通过accept函数拿到connfd
 * => TcpConnection设置回调 => 设置到Channel => Poller => Channel回调
 **/
class TcpConnection : nocopyable , public std::enable_shared_from_this<TcpConnection>
{
public:
    TcpConnection(EventLoop *loop, 
                  const std::string &name,
                  int sockfd, 
                  const InetAddress &localAddr,
                  const InetAddress &peerAddr);
    
    ~TcpConnection();

    void send(const std::string &buf); // 发送数据
    void sendFile(int filefd, off_t off, size_t count); // 发送文件
    void shutdown(); // 关闭连接
    void connectEstablished(); // 连接建立
    void connectDestroyed(); // 连接销毁

    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& locallAddr() const { return localAddr_; }
    const InetAddress& peerAddr() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }
    

    void setConnectionCallback(const ConnectionCallback &cb) { connectionCallback_ = cb; }
    void setMessageCallback(const MessageCallback &cb) { messageCallback_ = cb; }
    void setCloseCallback(const CloseCallback &cb) { closeCallback_ = cb; }
    void setWriteCompleteCallback(const WriteCompleteCallback &cb) { writeCompleteCallback_ = cb; }
    void setHighWaterMarkCallback(const HighWaterMarkCallback &cb, size_t highWaterMark) {
        highWaterMarkCallback_ = cb;
        highWaterMark_ = highWaterMark;
    }

private:
    enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };

    void handleRead(Timestamp recvTime); // 读事件回调
    void handleWrite(); // 写事件回调
    void handleClose(); // 关闭事件回调
    void handleError(); // 错误事件回调

    void sendInLoop(const void *message, size_t len);
    void shutdownInLoop();
    void sendFileInLoop(int filefd, off_t off, size_t count);

    void setState(StateE state) { state_ = state; }

    EventLoop *loop_; // 事件循环， 不是mainloop，TCPconn都是subloop的
    std::string name_; // 连接的名字
    std::atomic_int state_;  // 连接的状态
    bool reading_;  // 是否正在读数据

    std::unique_ptr<Socket> socket_;  // 该连接的socket
    std::unique_ptr<Channel> channel_;  // 该连接的channel
    const InetAddress localAddr_;  // 本地地址
    const InetAddress peerAddr_;   // 对端地址

    ConnectionCallback connectionCallback_;  // 有新连接时的回调
    MessageCallback messageCallback_;  // 有读写消息时的回调
    WriteCompleteCallback writeCompleteCallback_;  // 消息发送完成后的回调
    CloseCallback closeCallback_;  // 连接关闭时的回调
    HighWaterMarkCallback highWaterMarkCallback_;  // 高水位回调

    size_t highWaterMark_;  // 高水位标记
    Buffer inputBuffer_;  // 读缓冲区
    Buffer outputBuffer_; // 写缓冲区
};