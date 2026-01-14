#include "Poller.h"
#include "EPollPoller.h"

#include <stdlib.h>

 Poller* Poller::newDefaultPoller(EventLoop *loop){
    if(::getenv("MUDUO_US_POLL")){
        return nullptr;     // 使用Poll
    }else{
        return new EPollPoller(loop);
    }
 }