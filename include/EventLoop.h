#pragma once

#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>


#include "TimeStamp.h"
#include "Nocopyable.h"
#include "CurrentThread.h"
#include "Channel.h"
#include "TimerId.h"
#include "Callbacks.h"

class Poller;
class Channel;
class TimerQueue;

class EventLoop :nocopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    void loop();  // 启动事件循环
    void quit();  // 退出事件循环
    void runInLoop(Functor cb);  // 在当前loop中执行cb
    void queueInLoop(Functor cb);  // 把cb放入队列, 唤醒loop所在的线程执行cb
    void wakeup();  // 唤醒loop所在的线程

    // EventLoop的方法 => Poller的方法
    void updateChannel(Channel *channel);
    void removeChannel(Channel *channel);
    bool hasChannel(Channel *channel);

    // 时间相关的函数
    TimerId runAt(Timestamp time, TimerCallback cb);
    TimerId runAfter(double delay, TimerCallback cb);
    TimerId runEvery(double interval, TimerCallback cb);
    void cancel(TimerId timerId);

    Timestamp pollReturnTime() const { return pollReturnTime_;}

    /**
     * 判断EventLoop对象是否在自己的线程里
     * threadId_为EventLoop创建时的线程id CurrentThread::tid()为当前线程id
     */
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid();}

private:
    /**
     * 给eventfd返回的文件描述符wakeupFd_绑定的事件回调 
     * 当wakeup()时 即有事件发生时,调用handleRead()读wakeupFd_的8字节 
     * 同时唤醒阻塞的epoll_wait
     */
    void handleRead(); 
    void doPendingFunctors();  // 执行回调操作

    using ChannelList = std::vector<Channel *>;

    std::atomic_bool looping_;  // 标识事件循环是否启动
    std::atomic_bool quit_;    // 标识退出事件循环
    const pid_t threadId_;  // 记录当前EventLoop的所属线程id

    Timestamp pollReturnTime_;  // poller返回发生事件的channels的时间点
    std::unique_ptr<Poller> poller_;
    std::unique_ptr<TimerQueue> timerQueue_;

    int wakeupFd_;  // 作用：当mainLoop获取一个新用户的Channel 需通过轮询算法选择一个subLoop 通过该成员唤醒subLoop处理Channel
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;    // 返回Poller检测到当前有事件发生的所有Channel列表

    std::atomic_bool callingPendingFunctors_;   // 标识当前loop是否有需要执行的回调操作, 这里区别于io操作
    std::vector<Functor> pendingFunctors_;  // 存储loop需要执行的所有回调操作
    std::mutex mutex_;  // 保护上面vector的线程安全操作
};