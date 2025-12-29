#pragma once

#include <mutex>
#include <string>

#include "Thread.h"
#include "Nocopyable.h"

class EventLoopThread : nocopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThread(const ThreadInitCallback &cb = ThreadInitCallback(),
                    const std::string &name = std::string());
    ~EventLoopThread();

    EventLoop *startLoop();

private:
    coid threadFunc();
    
    EventLoop *loop_;
    bool exiting_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
    ThreadInitCallback callback_;
};