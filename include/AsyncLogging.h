#pragma once

#include "Thread.h"
#include "Nocopyable.h"
#include "LogFile.h"
#include "LogSream.h"
#include "FixedBuffer.h"

#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

class AsyncLogging : nocopyable
{
public:
    AsyncLogging(const std::string& basename, 
                off_t rollSize,
                int flushInterval = 3);
    
    ~AsyncLogging(){
        if(running_){
            stop();
        }
    }

    void append(const char* logline, int len);

    void start(){
        running_ = true;
        thread_.start();
    }

    void stop(){
        running_ = false;
        cond_.notify_one();
    }

private:
    using LargeBuffer = FixedBuffer<kLargeBuffer>;
    using BufferVector = std::vector<std::unique_ptr<LargeBuffer>>;
    using BufferPtr = std::unique_ptr<LargeBuffer>;

    void threadFunc();

    const int flushInterval_; // 刷新间隔时间，单位秒
    std::atomic<bool> running_; // 标志异步日志系统是否在运行
    const std::string basename_;
    const off_t rollSize_;
    Thread thread_; // 后台线程，用于写日志
    std::mutex mutex_;
    std::condition_variable cond_;

    BufferPtr currentBuffer_; // 当前正在使用的缓冲区
    BufferPtr nextBuffer_;    // 预备的下一个缓冲区
    BufferVector buffers_;    // 已满的缓冲区列表，等待写入磁盘
};