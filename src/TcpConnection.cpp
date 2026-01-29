#include <functional>
#include <string>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/sendfile.h>


#include "TcpConnection.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if (loop == nullptr)
    {
        LOG_FATAL << " mainLoop is null!";
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop, 
                const std::string &name,
                int sockfd, 
                const InetAddress &localAddr,
                const InetAddress &peerAddr)
            :loop_(CheckLoopNotNull(loop)),
            name_(name),
            state_(kConnecting),
            reading_(true),
            socket_(new Socket(sockfd)),
            channel_(new Channel(loop, sockfd)),
            localAddr_(localAddr),
            peerAddr_(peerAddr),
            highWaterMark_(64 * 1024 * 1024) // 64MB
{
    // 给channel设置相应的回调函数, poller给channel通知感兴趣的事件发生了, channel会回调相应的回调函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead, this, std::placeholders::_1)
    );
    channel_->setWtriteCallback(
        std::bind(&TcpConnection::handleWrite, this)
    );
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose, this)
    );
    channel_->setErrCallback(
        std::bind(&TcpConnection::handleError, this)
    );

    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection(){
    LOG_INFO << "TcpConnection::dtor[" << name_ << "] at fd=" << channel_->fd() << " state=" << (int)state_ << "\n";
}

void TcpConnection::send(const std::string &buf){
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread()){
            sendInLoop(buf.c_str(), buf.size());
        }
        else{
            loop_->runInLoop(
                std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size())
            );
        }
    }
}

void TcpConnection::sendFile(int filefd, off_t off, size_t count){
    if(connected())
    {
        if(loop_->isInLoopThread()){
            sendFileInLoop(filefd, off, count);
        }
        else{
            loop_->runInLoop(
                std::bind(&TcpConnection::sendFileInLoop, this, filefd, off, count)
            );
        }
    }
    else {
        LOG_ERROR << "TcpConnection is not connected, give up sendFile\n";
    }
}

void TcpConnection::shutdown(){
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(
            std::bind(&TcpConnection::shutdownInLoop, this)
        );
    }
}

void TcpConnection::connectEstablished(){
    setState(kConnected);

    // 防止channel被手动remove掉 channel还在执行回调操作
    channel_->tie(shared_from_this());
    channel_->enableReading(); // 启动channel的读事件监听

    // 新连接建立，调用用户注册的回调操作
    connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed(){
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll(); // 停止channel的所有事件监听
        connectionCallback_(shared_from_this());
    }

    channel_->remove(); // 把channel从Poller中删除掉
}

// EventLoop 的 Poller 检测到这个 TcpConnection 对应的 socket 文件描述符上有可读事件 (EPOLLIN)。
// EventLoop 遍历有事件的 Channel 列表，找到与这个 socket 关联的 channel_。
// EventLoop 调用 channel_ 注册的“读回调”，而这个回调函数就是 TcpConnection::handleRead。
void TcpConnection::handleRead(Timestamp recvTime){
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0)   // 有数据到达
    {
        // 已建立连接的用户有可读事件发生了, 调用用户传入的回调操作onMessage回调
        // 将新接收数据的缓冲区指针传递给用户。用户代码会从这个 buffer 中读取和消费数据。
        messageCallback_(shared_from_this(), &inputBuffer_, recvTime);
    } 
    else if (n == 0)    // 连接断开
    {
        handleClose();
    } 
    else    // 出错
    {
        errno = savedErrno;
        LOG_ERROR << "TcpConnection::handleRead() err:" << errno << "\n";
        handleError();
    }
}

// 当 EventLoop 通知我们TCP内核发送缓冲区有空间时，
// 把我们自己应用层缓冲区 (outputBuffer_) 里积压的数据发送出去。
void TcpConnection::handleWrite(){
    if(channel_->isWriting()){
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n >0)
        {
            outputBuffer_.retrieve(n);   // 复位缓冲区

            // 一旦我们的缓冲区空了，必须停止监听写事件
            if(outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_, shared_from_this())
                    );
                }
            }
            
            if(state_ == kDisconnecting){
                shutdownInLoop();
            }
        }
        else {
            LOG_ERROR << "TcpConnection::handleWrite() err:" << savedErrno << "\n";
        }
    }
    else
    {
        LOG_ERROR << "TcpConnection fd=" << channel_->fd() << " is down, no more writing\n";
    }
}

// poller => channel::closeCallback_ => TcpConnection::handleClose
void TcpConnection::handleClose(){
    setState(kDisconnected);
    channel_->disableAll();

    // 调用用户注册的连接关闭回调
    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);
    closeCallback_(connPtr);    // TCPserver :: removeConnection 给的
}

void TcpConnection::handleError(){
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0){
        err = errno;
    }
    else {
        err = optval;
    }

    LOG_ERROR << "TcpConnection::handleError name:" << name_ << " - SO_ERROR:" << err << "\n";
}

void TcpConnection::sendInLoop(const void *message, size_t len){
    ssize_t nwrote = 0; // 已经发送的数据长度
    size_t remaing = len; // 剩余待发送的数据长度
    bool faultError = false;

    if(state_ == kDisconnected){
        LOG_ERROR << "TcpConnection::sendInLoop disconnected, give up writing\n";
        return;
    }

    // 表示channel_第一次开始写数据或者缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), message, len);
        if(nwrote >= 0)
        {
            remaing = len - nwrote;
            if(remaing == 0 && writeCompleteCallback_)
            {
                // 发送完成且用户注册了回调函数
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this())
                );
            }
        }
        else    // nwrote < 0 ，出错
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK){   // EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回 等同于EAGAIN
                LOG_ERROR << "TcpConnection::sendInLoop err:" << errno << "\n";
                if(errno == EPIPE || errno == ECONNRESET){ // SIGPIPE
                    //方已关闭连接的写端 或  socket 重置
                    faultError = true;
                }
            }
        }
    }

    /**
     * 说明当前这一次write并没有把数据全部发送出去 剩余的数据需要保存到缓冲区当中
     * 然后给channel注册EPOLLOUT事件，Poller发现tcp的发送缓冲区有空间后会通知
     * 相应的sock->channel，调用channel对应注册的writeCallback_回调方法，
     * channel的writeCallback_实际上就是TcpConnection设置的handleWrite回调，
     * 把发送缓冲区outputBuffer_的内容全部发送完成
     **/
    if(!faultError && remaing > 0)
    {
        size_t oldlen = outputBuffer_.readableBytes();
        if(oldlen + remaing >= highWaterMark_ &&
           oldlen < highWaterMark_ && highWaterMarkCallback_){
            // 达到高水位标记
            loop_->queueInLoop(
                std::bind(highWaterMarkCallback_, shared_from_this(), oldlen + remaing)
            );
        }

        outputBuffer_.append((const char*)message + nwrote, remaing);
        if(!channel_->isWriting()){
            // 这里一定要注册channel的写事件 否则poller不会给channel通知epollout
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdownInLoop(){
    if(!channel_->isWriting()){
        // 说明outputBuffer_的数据已经全部发送完毕 可以关闭写端了
        socket_->shutdownWrite();
    }
}

void TcpConnection::sendFileInLoop(int filefd, off_t off, size_t count){
    ssize_t nwrote = 0;
    size_t remaining = count;
    bool faultError = false;

    if(state_ == kDisconnected){
        LOG_ERROR << "TcpConnection::sendFileInLoop disconnected, give up writing\n";
        return;
    }

    // 表示channel_第一次开始写数据或者缓冲区没有待发送数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0 ){
        nwrote = ::sendfile(channel_->fd(), filefd, &off, count);
        if(nwrote >= 0)
        {
            remaining = count - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                // 发送完成且用户注册了回调函数
                // 既然在这里数据全部发送完成，就不用再给channel设置epollout事件了
                loop_->queueInLoop(
                    std::bind(writeCompleteCallback_, shared_from_this())
                );
            }
        }
        else   // nwrote < 0 ，出错
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK){   // EWOULDBLOCK表示非阻塞情况下没有数据后的正常返回 等同于EAGAIN
                LOG_ERROR << "TcpConnection::sendFileInLoop err:" << errno << "\n";
                if(errno == EPIPE || errno == ECONNRESET){ // SIGPIPE
                    //socket 重置
                    faultError = true;
                }
            }
        }
    }

    // 处理剩余数据
    if(!faultError && remaining > 0)
    {
        loop_->runInLoop(
            std::bind(&TcpConnection::sendFileInLoop, this, filefd, off, remaining)
        );
    }
}
    