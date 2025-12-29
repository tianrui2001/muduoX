#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

#include "Nocopyable.h"

class EventLoop;
class EventLoopThread;

class EventLoopThreadPool : nocopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop *)>;

    EventLoopThreadPool(EventLoop *baseLoop, const std::string &nameArg);
    ~EventLoopThreadPool();

    void start(const ThreadInitCallback &cb = ThreadInitCallback());

    // 如果工作在多线程中，baseLoop_(mainLoop)会默认以轮询的方式分配Channel给subLoop
    EventLoop* getNextLoop();

    // 获取所有的EventLoop
    std::vector<EventLoop *> getAllLoops(); 

    bool started() const { return started_; }
    const std::string& name() const { return name_; }
    void setThreadNum(int numThreads) { numThreads_ = numThreads; }
private:
    EventLoop *baseLoop_;  // 主Reactor线程的EventLoop对象
    std::string name_;  // 线程池名称
    bool started_;  // 线程池是否启动
    int numThreads_;  // 线程池中的线程数量
    int next_;  // 新连接到来，所选择EventLoop的索引
    std::vector<std::unique_ptr<EventLoopThread>> threads_; //IO线程的列表
    std::vector<EventLoop *> loops_; //IO线程的EventLoop对象列表, 指向的是EVentLoopThread线程函数创建的EventLoop对象。

};