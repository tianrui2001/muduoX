#pragma once

#include <atomic>

#include "Nocopyable.h"
#include "Callbacks.h"
#include "TimeStamp.h"

class Timer : nocopyable 
{

public:
    Timer(TimerCallback cb, Timestamp when, double interval)
        : callback_(std::move(cb)),
            expiration_(when),
            interval_(interval),
            repeat_(interval > 0.0),
            sequence_(s_numCreated_++) {}
    
    void run() const { callback_(); }
    Timestamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    void restart(Timestamp now);

    static int64_t numCreated() { return s_numCreated_.load(); }
private:

    const TimerCallback callback_;
    Timestamp expiration_;   // 超时时间点
    const double interval_;  // 超时时间间隔
    bool repeat_;            // 是否重复定时
    const int64_t sequence_;  // 一个全局唯一的、自增的序列号, Timer 对象的唯一身份ID。

    // 用来生成上面那个唯一的 sequence_, Atomic 意味着多线程环境下的自增操作是安全的。
    static std::atomic<int64_t> s_numCreated_;
};