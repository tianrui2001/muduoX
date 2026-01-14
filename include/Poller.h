#pragma once

#include <unordered_map>
#include <vector>

#include "EventLoop.h"
#include "Channel.h"
#include "TimeStamp.h"

class EventLoop;

// muduo库中多路事件分发器的核心IO复用模块
class Poller {
public:

    using ChannelList = std::vector<Channel *>;

    Poller(EventLoop *loop) : ownerLoop_(loop) {}
    virtual ~Poller() = default;

   // 给所有IO复用保留统一的接口
   virtual Timestamp poll(int timeoutMs, ChannelList *activeChannels) = 0;
   virtual void updateChannel(Channel *channel) = 0;
   virtual void removeChannel(Channel *channel) = 0;

   // 判断参数channel是否在当前的Poller当中
   bool hasChannel(Channel *channel) const {
        auto it = channels_.find(channel->fd());
        return it != channels_.end() && it->second == channel;
   }

   // EventLoop可以通过该接口获取默认的IO复用的具体实现
   static Poller* newDefaultPoller(EventLoop *loop);

protected:
    using ChannelMap = std::unordered_map<int, Channel *>;
    ChannelMap channels_;  // fd到channel的映射表
private:
    // 定义Poller所属的事件循环EventLoop
    EventLoop *ownerLoop_;
};