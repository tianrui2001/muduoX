#pragma once

#include "Timer.h"

class TimerId {
public:
    TimerId()
        : timer_(nullptr),
            sequence_(0) {}
    
    TimerId(Timer* timer, int64_t seq)
        :timer_(timer),
            sequence_(seq) {}

private:
    friend class TimerQueue;

    // Timer 对象的所有权完全由 TimerQueue 管理。TimerId 只是一个临时的、非拥有的引用。
    Timer* timer_;

    // 用于标识同一个Timer对象的不同实例
    int64_t sequence_; 
};