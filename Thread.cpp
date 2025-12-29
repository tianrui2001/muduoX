#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_ = 0;

explicit Thread::Thread(ThreadFunc func, const std::string &name = std::string())
    : started_(false),
      joined_(false),
      threadPtr_(nullptr),
      tid_(0),
      func_(std::move(func)),
      name_(name)
{
    setDefaultName();
}

Thread::~Thread(){
    if(started_ && !joined_){
        threadPtr_->detach();
    }
}

void Thread::start(){
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);

    // 创建线程
    threadPtr_ = std::shared_ptr<std::thread>(new std::thread([this](){
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        func_();
    }));

    // 这里必须等待获取上面新创建的线程的tid值
    sem_wait(&sem);
}

void Thread::join(){
    joined_ = true;
    threadPtr_->join();
}

void Thread::setDefaultName(){
    int num = ++numCreated_;
    if(name_.empty()){
        name_ = "Thread" + std::to_string(num);
    }
}

