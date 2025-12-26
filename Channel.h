#pragma once

#include "Nocopyable.h"
#include "TimeStamp.h"

#include <memory>
#include <functional>

class EventLoop;


/**
 * 理清楚 EventLoop、Channel、Poller之间的关系  Reactor模型上对应多路事件分发器
 * Channel理解为通道 封装了sockfd和其感兴趣的event 如EPOLLIN、EPOLLOUT事件 还绑定了poller返回的具体事件
 **/
class Channel : public nocopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadCallback = std::function<void(Timestamp)>;

    Channel(EventLoop *loop, int fd);
    ~Channel();
    

    // fd得到Poller通知以后处理事件, handleEvent在EventLoop::loop()中调用
    void handleEvent(Timestamp recvTime);

    // 防止当channel被手动remove掉 channel还在执行回调操作
    void tie(const std::shared_ptr<void> &);

    void remove();


    int fd() const { return fd_; }
    int events() const { return events_; }
    int index() const { return index_; }
    EventLoop* ownerLoop() { return loop_; }    // one loop per thread
    void set_index(int index) { index_ = index; }
    void set_revents(int revt) { revents_ = revt; }

    // 设置回调函数对象
    void setReadCallback(ReadCallback cb) { readCallback_ = std::move(cb);  update(); }
    void setWtriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); update(); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); update(); }
    void setErrCallback(EventCallback cb) {errCallback_ = std::move(cb); update(); }

    // 返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    // 设置fd相应的事件状态 相当于epoll_ctl add delete
    void enableReading() { events_ |= kReadEvent; update(); }
    void diableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() {events_ = kNoneEvent; update(); }

private:
    void update();  // 更新channel所表示的fd的events事件
    void handleEventWithGuard(Timestamp recvTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;
    
    EventLoop *loop_;   // 事件循环
    const int fd_;        // fd，Poller监听的对象
    int events_;     // fd 感兴趣的事件             
    int revents_;     // 实际发生的事件
    int index_;     // 用于记录该通道在 poller 中的状态

    std::weak_ptr<void> tie_;
    bool tied_;

    ReadCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errCallback_;
};