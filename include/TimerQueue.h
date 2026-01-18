#pragma once

#include <set>
#include <vector>
#include <utility>

#include "Nocopyable.h"
#include "Channel.h"
#include "TimeStamp.h"
#include "Callbacks.h"

class EventLoop;
class Timer;
class TimerId;

// 管理一大堆 Timer 对象（闹钟）。
// 找出下一个最快到期的 Timer。
// 利用 timerfd 请求内核在那个最快的时间点唤醒自己。
// 被唤醒后，执行所有已经到期的 Timer 的回调函数。
// 高效、线程安全地处理添加和取消定时器的请求。
class  TimerQueue : nocopyable
{
public:
    explicit TimerQueue(EventLoop* loop);
    ~ TimerQueue();

    // 添加一个定时器，必须是线程安全的，经常被其他线程调用
    TimerId addTimer(TimerCallback cb, Timestamp when, double interval);

    void cancel(TimerId timerId);

private:
    using Entry = std::pair<Timestamp, Timer*>;
    using TimerList = std::set<Entry>;
    using ActiveTimer = std::pair<Timer*, int64_t>;
    using ActiveTimerSet = std::set<ActiveTimer>;

    void addTimerInLoop(Timer* timer);
    void cancelInLoop(TimerId timerId);
    
    // timerfdChannel_ 在 loop_ 的 Poller 中注册的“读”事件。
    void handleRead();

    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);
    bool insert(Timer* timer);


    EventLoop* loop_;           // 所属的事件循环
    const int timerfd_;         // Linux提供的高性能定时器接口
    Channel timerfdChannel_;    // 一个封装了 timerfd_ 的 Channel 对象

    TimerList timers_;              // 按时间顺序存储了所有的 Timer
    ActiveTimerSet activeTimers_;   // 按 Timer 地址排序的 Timer 集合
    bool callingExpiredTimers_;     // 表示 TimerQueue 当前是否正在执行到期定时器的回调函数
    ActiveTimerSet cancelingTimers_;    // 来存放在 callingExpiredTimers_ = true 期间被请求取消的 Timer
};
