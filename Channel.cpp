#include "Channel.h"
#include "TimeStamp.h"
#include "Logger.h"

#include <sys/epoll.h>

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::kWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    :loop_(loop),
    fd_(fd),
    events_(0),
    revents_(0),
    index_(-1),
    tied_(false) {}

Channel::~Channel(){}

void Channel::handleEvent(Timestamp recvTime){
    if(tied_){
        std::shared_ptr<void> guard = tie_.lock();
        if(guard){
            handleEventWithGuard(recvTime);
        }
        // weak_ptr提升失败，说明tie的对象已经被析构，不能执行回调函数
    }else{
        handleEventWithGuard(recvTime);
    }
}


// channel的tie方法什么时候调用过?  TcpConnection => channel
/**
 * TcpConnection中注册了Channel对应的回调函数，传入的回调函数均为TcpConnection
 * 对象的成员方法，因此可以说明一点就是：Channel的结束一定晚于TcpConnection对象！
 * 此处用tie去解决TcpConnection和Channel的生命周期时长问题，从而保证了Channel对象能够在
 * TcpConnection销毁前销毁。
 **/
void Channel::tie(const std::shared_ptr<void> &obj){
    // 用weak_ptr来引用shared_ptr， 防止循环引用
    tie_ = obj;
    tied_ = true;
}


//update 和remove => EpollPoller 更新channel在poller中的状态
/**
 * 当改变channel所表示的fd的events事件后，update负责再poller里面更改fd相应的事件epoll_ctl
 **/
void Channel::remove(){
    loop_->removeChannel(this);
}

void Channel::update(){
    loop_->updateChannel(this);
}

void Channel::handleEventWithGuard(Timestamp recvTime){
    LOG_INFO("Channel::handleEventWithGuard revents:%d\n", revents_);

    // 关闭事件 优先处理
    if(revents_ & EPOLLHUP && !(revents_ & EPOLLIN)){
        if(closeCallback_){
            closeCallback_();
        }
    }

    // 错误事件
    if(revents_ & EPOLLERR){
        if(errCallback_){
            errCallback_();
        }
    }

    // 读事件
    if(revents_ & (EPOLLIN | EPOLLPRI)){
        if(readCallback_){
            readCallback_(recvTime);
        }
    }

    // 写事件
    if(revents_ & EPOLLOUT){
        if(writeCallback_){
            writeCallback_();
        }
    }
}