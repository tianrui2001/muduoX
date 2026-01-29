#include <string.h>
#include <unistd.h>

#include "EPollPoller.h"
#include "EventLoop.h"
#include "Channel.h"
#include "Logger.h"

const int kNew = -1;    // 某个channel还没添加至Poller          // channel的成员index_初始化为-1
const int kAdded = 1;   // 某个channel已经添加至Poller
const int kDeleted = 2; // 某个channel已经从Poller删除

EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop),
    epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
    events_(kInitEventListSize) {
        if(epollfd_ < 0){
            LOG_FATAL << "EPollPoller:: epoll_create1 error:" << errno << "\n";
        }
}

EPollPoller::~EPollPoller(){
    ::close(epollfd_);
}


Timestamp EPollPoller::poll(int timeoutMs, ChannelList *activeChannels){
    int numEvents = ::epoll_wait(epollfd_,
                                 &*events_.begin(),
                                 static_cast<int>(events_.size()),
                                 timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());

    if(numEvents > 0){
        fillActiveChannels(numEvents, activeChannels);
        if(static_cast<size_t>(numEvents) == events_.size()){
            events_.resize(events_.size() * 2); // 扩容
        }
    }else if(numEvents == 0){   // 超时
        LOG_DEBUG <<  " => nothing happened";
    }else { // 出错
        if(savedErrno != EINTR){    // 不是被信号中断, 中断可不用管
            errno = savedErrno;
            LOG_ERROR << "EPollPoller::poll() err:" << savedErrno << "\n";
        }
    }

    return now;
}

void EPollPoller::updateChannel(Channel *channel){
    const int index = channel->index();

    if(index == kNew || index == kDeleted){
        int fd = channel->fd();
        if(index == kNew){
            channels_[fd] = channel;
        }

        channel->set_index(kAdded);
        update(EPOLL_CTL_ADD, channel);
    } else if (index == kAdded){
        int fd = channel->fd();
        if(channel->isNoneEvent()){
            update(EPOLL_CTL_DEL, channel);
            channel->set_index(kDeleted);
        }
        else{
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel *channel){
    int fd = channel->fd();

    channels_.erase(fd);

    int index = channel->index();
    if(index == kAdded){
        update(EPOLL_CTL_DEL, channel);
    }

    channel->set_index(kNew);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList *activeChannels) const {
    for(int i=0; i<numEvents; i++){
        auto channel = static_cast<Channel *>(events_[i].data.ptr);
        channel->set_revents(events_[i].events);
        activeChannels->push_back(channel);
    }
}

void EPollPoller::update(int operation, Channel *channel){
    epoll_event event;
    memset(&event, 0, sizeof(event));
    int fd = channel->fd();

    event.events = channel->events();
    event.data.ptr = channel;

    if(::epoll_ctl(epollfd_, operation, fd, &event) < 0){
        if(operation == EPOLL_CTL_DEL){
            LOG_ERROR << "EPollPoller::update() EPOLL_CTL_DEL error:" << errno << "\n";
        } 
        else{
            LOG_FATAL << "EPollPoller::update() epoll_ctl error:" << errno << "\n";
        }
    }
}