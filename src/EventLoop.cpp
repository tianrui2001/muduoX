#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"
#include "TimerQueue.h"
#include "IOuring.h"

#include <sys/eventfd.h>
#include <vector>
#include <errno.h>
#include <unistd.h>
#include <memory>

// 防止一个线程创建多个EventLoop
__thread EventLoop *t_loopInThisTread = nullptr;

const int kPollTimeMs = 10000; // 10s

/* 创建线程之后主线程和子线程谁先运行是不确定的。
 * 通过一个eventfd在线程之间传递数据的好处是多个线程无需上锁就可以实现同步。
 * eventfd支持的最低内核版本为Linux 2.6.27,在2.6.26及之前的版本也可以使用eventfd，但是flags必须设置为0。
 * 函数原型：
 *     #include <sys/eventfd.h>
 *     int eventfd(unsigned int initval, int flags);
 * 参数说明：
 *      initval,初始化计数器的值。
 *      flags, EFD_NONBLOCK,设置socket为非阻塞。
 *             EFD_CLOEXEC，执行fork的时候，在父进程中的描述符会自动关闭，子进程中的描述符保留。
 * 场景：
 *     eventfd可以用于同一个进程之中的线程之间的通信。
 *     eventfd还可以用于同亲缘关系的进程之间的通信。
 *     eventfd用于不同亲缘关系的进程之间通信的话需要把eventfd放在几个进程共享的共享内存中（没有测试过）。
 */
// 创建wakeupfd 用来notify唤醒subReactor处理新来的channel
static int createEventfd(){
    int eventfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if(eventfd < 0){
        LOG_FATAL << "eventfd error:" << errno << "\n";
    }
    return eventfd;
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      threadId_(CurrentThread::tid()),
      poller_(Poller::newDefaultPoller(this)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)),
      callingPendingFunctors_(false),
      uringManager_(new UringManager(this)) 
{
    if(t_loopInThisTread){
        LOG_FATAL << "Another EventLoop " << t_loopInThisTread << " exists in this thread " << threadId_ << "\n";
    } else {
        t_loopInThisTread = this;  // 记录当前线程的EventLoop
    }

    // 绑定wakeupChannel_的事件到Poller中
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead, this));
    wakeupChannel_->enableReading();    // 注册到Poller
}

EventLoop::~EventLoop(){
    wakeupChannel_->disableAll();   // 给Channel移除所有感兴趣的事件
    wakeupChannel_->remove();   // 把Channel从EventLoop上删除掉
    ::close(wakeupFd_);
    t_loopInThisTread = nullptr;  // 清除当前线程的EventLoop
}

// 开启事件循环
void EventLoop::loop(){
    looping_ = true;
    quit_ = false;

    while(!quit_){
        activeChannels_.clear();
        // 监听事件，返回发生事件的channels
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);

        for(Channel *channel :activeChannels_){
            // Poller监听哪些channel发生了事件 然后上报给EventLoop 通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }

        doPendingFunctors();  // 执行回调操作
    }

    looping_ = false;
}

void EventLoop::quit(){
    quit_ = true;

    if(!isInLoopThread()){
        // 如果在非本线程调用quit 则唤醒loop所在的线程 执行quit
        wakeup();  
    }
}

// 在当前loop中执行cb
void EventLoop::runInLoop(Functor cb){
    if(isInLoopThread()){
        cb();
    } else {
        // 在非当前EventLoop线程中执行cb，就需要唤醒EventLoop所在线程执行cb
        queueInLoop(cb);
    }
}

void EventLoop::queueInLoop(Functor cb){
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    /**
     * callingPendingFunctors的意思是 当前loop正在执行回调中 但是loop的pendingFunctors_中又加入了新的回调 需要通过wakeup写事件
     * 唤醒相应的需要执行上面回调操作的loop的线程 让loop()下一次poller_->poll()不再阻塞（阻塞的话会延迟前一次新加入的回调的执行），然后
     * 继续执行pendingFunctors_中的回调函数
     **/
    if(!isInLoopThread() || callingPendingFunctors_){
        wakeup();
    }
}

// 用来唤醒loop所在线程 向wakeupFd_写一个数据 wakeupChannel就发生读事件 当前loop线程就会被唤醒
void EventLoop::wakeup(){
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8\n";
    }
}

void EventLoop::handleRead(){
    uint64_t one = 1;
    ssize_t n = ::read(wakeupFd_, &one, sizeof one);
    if(n != sizeof one){
        LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8\n";
    }
}

void EventLoop::updateChannel(Channel *channel){
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel){
    poller_->removeChannel(channel);
}


bool EventLoop::hasChannel(Channel *channel){
    return poller_->hasChannel(channel);
}


void EventLoop::doPendingFunctors(){
    std::vector<Functor> functors;

    callingPendingFunctors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        // 交换functor和pendingFunctors_， 防止运行回调时阻塞时间太长，提高效率
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor : functors){
        functor();  // 执行当前loop需要执行的回调操作
    }

    callingPendingFunctors_ = false;
}


TimerId EventLoop::runAt(Timestamp time, TimerCallback cb){
    return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback cb){
    Timestamp time(addTime(Timestamp::now(), delay));
    return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback cb){
    Timestamp time(addTime(Timestamp::now(), interval));
    return timerQueue_->addTimer(std::move(cb), time, interval);
}

void EventLoop::cancel(TimerId timerId){
    timerQueue_->cancel(timerId);
}