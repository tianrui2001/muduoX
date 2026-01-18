#include "TimerQueue.h"
#include "Logger.h"
#include "EventLoop.h"
#include "Timer.h"
#include "TimerId.h"

#include <sys/timerfd.h>
#include <unistd.h>
#include <cstring>

int createTimerfd(){
    // CLOCK_MONOTONIC: 一个只能向前、不能后退、永远不会跳变的“流逝时间”计数器
    int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if(timerfd < 0){
        LOG_FATAL("Failed in timerfd_create");
    }

    return timerfd;
}

struct timespec howMuchTimeFromNow(Timestamp when){
    int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();

    // 100 微秒是一个经验值,避免无效的立即超时和过于频繁的系统调用
    if(microseconds < 100){
        microseconds = 100;
    }

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(microseconds % Timestamp::kMicroSecondsPerSecond * 1000);
    return ts;
}

// 读出从上次成功读取之后，这个定时器“超时”的总次数
void readTimerfd(int timerfd, Timestamp now){
    uint64_t howmany;
    ssize_t n = ::read(timerfd, &howmany, sizeof howmany);
    if(n != sizeof howmany){
        LOG_ERROR("TimerQueue::handleRead() reads %lu bytes instead of 8\n", n);
    }
}

// 根据给定的未来到期时间点，来重置timerfd 的下一次超时。
void resetTimerfd(int timerfd, Timestamp expiration){
    struct itimerspec newValue;
    struct itimerspec oldValue;

    memset(&newValue, 0, sizeof newValue);
    memset(&oldValue, 0, sizeof oldValue);

    newValue.it_value = howMuchTimeFromNow(expiration);
    
    /**
     * 
     * 参数说明：timerfd_settime函数用来设置一个定时器的初始值和间隔值。
     *      timerfd：由timerfd_create创建的定时器文件描述符。
     *      flags：一般设为0，表示相对时间。如果设为TFD_TIMER_ABSTIME，则表示绝对时间。
     *      new_value：指向itimerspec结构体，指定定时器的初始值和间隔值。
     *      old_value：指向itimerspec结构体，用于存储定时器之前的设置，可以设为NULL。
     **/
    if(::timerfd_settime(timerfd, 0, &newValue, &oldValue) < 0){
        LOG_ERROR("timerfd_settime() error");
    }
}


TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop, timerfd_),
      timers_(),
      callingExpiredTimers_(false)
{
    timerfdChannel_.setReadCallback(
        std::bind(&TimerQueue::handleRead, this)
    );
    timerfdChannel_.enableReading();
}

TimerQueue::~TimerQueue(){
    timerfdChannel_.disableAll();
    timerfdChannel_.remove();
    ::close(timerfd_);

    for(const auto& timer : timers_){
        delete timer.second;
    }
}

TimerId TimerQueue::addTimer(TimerCallback cb, Timestamp when, double interval){
   Timer* timer = new Timer(std::move(cb), when, interval);
   loop_->runInLoop(
    std::bind(&TimerQueue::addTimerInLoop, this, timer)
   ); 
   return TimerId(timer, timer->sequence());
}

void TimerQueue::cancel(TimerId timerId){
    loop_->runInLoop(
        std::bind(&TimerQueue::cancelInLoop, this, timerId)
    );
}

void TimerQueue::addTimerInLoop(Timer* timer){
    bool earliestChanged = insert(timer);

    // 如果新插入的定时器是所有定时器中最早到期的一个，则需要重置 timerfd_，让它更早超时。
    if(earliestChanged){
        resetTimerfd(timerfd_, timer->expiration());
    }
}

// 取消定时器
void TimerQueue::cancelInLoop(TimerId timerId){
    ActiveTimer timer(timerId.timer_, timerId.sequence_);
    auto it = activeTimers_.find(timer);

    if(it != activeTimers_.end()){  // 在activeTimers_中找到了，在2个set中删除对应的项
        size_t n = timers_.erase(std::make_pair(it->first->expiration(), it->first));
        if(n != 1){
            LOG_ERROR("TimerQueue::cancelInLoop erase timer failed");
        }

        delete it->first;
        activeTimers_.erase(it);
    } else if(callingExpiredTimers_){
        // 找不到，可能是在执行回调函数，那么将timer加入取消列表
        cancelingTimers_.insert(timer);
    }
}

void TimerQueue::handleRead(){
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);

    std::vector<Entry> expired = getExpired(now);
    callingExpiredTimers_ = true;
    cancelingTimers_.clear();
    for(const auto& it : expired){
        it.second->run();
    }
    callingExpiredTimers_ = false;

    reset(expired, now);
}

// 在 timers_中划定一个“到期”的范围。将这个范围内的所有 Timer 复制到一个临时的 expired 列表中。
// 从 timers_ 和 activeTimers_ 两个管理集合中彻底删除这些到期的 Timer,返回临时的 expired 列表
std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now){
    std::vector<Entry> expired;
    Entry sentry(now, reinterpret_cast<Timer*>(UINTPTR_MAX));   // 哨兵
    auto end = timers_.lower_bound(sentry); // 找到第一个未到期的定时器
    std::copy(timers_.begin(), end, back_inserter(expired));
    timers_.erase(timers_.begin(), end);    // 从timers_中删除到期的定时器

    for(const auto& it : expired){
        ActiveTimer timer(it.second, it.second->sequence());
        size_t n = activeTimers_.erase(timer);
        if(n != 1){
            LOG_ERROR("TimerQueue::getExpired erase timer failed");
        }
    }

    return expired;
}

// 重新设置定时器。 删除掉不是重复的定时器，重新插入重复的定时器。 获取下一次超时的时间
void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now){
    Timestamp nextExpire;

    for(const auto& it : expired){
        ActiveTimer timer(it.second, it.second->sequence());
        // 如果是重复定时器，则重新设置它的到期时间，并插入到定时器集合中
        if(it.second->repeat() && cancelingTimers_.find(timer) == cancelingTimers_.end()){
            it.second->restart(now);
            insert(it.second);
        } else {
            // 非重复的 timer ，直接删除
            delete it.second;
        }
    }

    if(!timers_.empty()){
        nextExpire = timers_.begin()->first;
    }

    if(nextExpire.valid()){
        resetTimerfd(timerfd_, nextExpire);
    }
}

// 返回值：新插入的 Timer 是否改变了“最早到期时间”
bool TimerQueue::insert(Timer* timer){
    bool earliestChanged =false;
    Timestamp when = timer->expiration();
    auto it = timers_.begin();

    // 判断新插入的定时器是否是最早到期的定时器
    if(it == timers_.end() || when < it->first){
        earliestChanged = true;
    }

    timers_.insert(std::make_pair(when, timer));
    activeTimers_.insert(ActiveTimer(timer, timer->sequence()));
    return earliestChanged;
}