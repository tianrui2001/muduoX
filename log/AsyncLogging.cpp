#include "AsyncLogging.h"

AsyncLogging::AsyncLogging(const std::string& basename, 
            off_t rollSize,
            int flushInterval)
    : flushInterval_(flushInterval),
      running_(false),
      basename_(basename),
      rollSize_(rollSize),
      thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
      currentBuffer_(new LargeBuffer),
      nextBuffer_(new LargeBuffer)
{
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16); // 只维持队列长度2~16.
}


void AsyncLogging::append(const char* logline, int len){
    std::lock_guard<std::mutex> lock(mutex_);
    if(currentBuffer_->avail() > len){
        currentBuffer_->append(logline, len);
    } else {
        buffers_.push_back(std::move(currentBuffer_));
        if(nextBuffer_){
            currentBuffer_ = std::move(nextBuffer_);
        } else {
            currentBuffer_.reset(new LargeBuffer); // 备用缓冲区也没有了，只能新建
        }

        currentBuffer_->append(logline, len);
        cond_.notify_one(); // 通知后台线程，有数据可写
    }
}

void AsyncLogging::threadFunc(){
    // output写入磁盘接口
    LogFile output(basename_, rollSize_, flushInterval_);
    BufferPtr newBuffer1(new LargeBuffer);
    BufferPtr newBuffer2(new LargeBuffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);

    while(running_){
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if(buffers_.empty()){   // 没有日志可写，等待
                cond_.wait_for(lock, std::chrono::seconds(flushInterval_));
            }

            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            if(!nextBuffer_){
                nextBuffer_ = std::move(newBuffer2);
            }
            buffersToWrite.swap(buffers_); // 交换, 避免长时间持有锁
        }

        for(const auto& buffer : buffersToWrite){
            output.append(buffer->data(), buffer->length());
        }

        if(buffersToWrite.size() > 2){
            buffersToWrite.resize(2);
        }

        // 复用缓冲区
        if(!newBuffer1){
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if(!newBuffer2){
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }

        buffersToWrite.clear();
        output.flush(); // 清空文件夹缓冲区
    }

    output.flush(); // 确保一定清空。
}