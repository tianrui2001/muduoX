// #include <muduoX/TcpServer.h>
// #include <muduoX/Logger.h>
// #include <string>
// #include <functional>


// class EchoService{
// public:
//     EchoService(EventLoop *loop,
//                 const InetAddress &listenAddr,
//                 const std::string &name)
//         :loop_(loop),
//         server_(loop, listenAddr, name)
//     {
//         server_.setConnectionCallback(
//             std::bind(&EchoService::onConnection, this, std::placeholders::_1)
//         );
//         server_.setMessageCallback(
//             std::bind(&EchoService::onMessage, this,
//                       std::placeholders::_1,
//                       std::placeholders::_2,
//                       std::placeholders::_3)
//         );

//         server_.setThreadNum(4); // 设置底层subLoop的数量
//     }

//     void start(){
//         server_.start();
//     }


// private:
//     // 连接回调函数
//     void onConnection(const TcpConnectionPtr &conn){
//         if(conn->connected()){
//             LOG_INFO("EchoService - %s -> %s is online", 
//                 conn->peerAddr().toIpPort().c_str(), 
//                 conn->locallAddr().toIpPort().c_str());
//         } else {
//             LOG_INFO("EchoService - %s -> %s is offline", 
//                 conn->peerAddr().toIpPort().c_str(), 
//                 conn->locallAddr().toIpPort().c_str());
//         }
//     }

//     // 消息回调函数
//     void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp timestamp){
//         std::string msg = buf->retrieveAllAsString();
//         conn->send(msg);
//         conn->shutdown();   // 关闭连接，发送完数据后关闭连接
//     }

//     EventLoop *loop_;
//     TcpServer server_;
// };

// int main(){
//     EventLoop loop;
//     InetAddress addr(8000);
//     EchoService echoService(&loop, addr, "EchoService");
//     echoService.start();
//     loop.loop();
// }


#include <muduoX/EventLoop.h>
#include <muduoX/EventLoopThread.h>
#include <muduoX/Thread.h>

#include <stdio.h>
#include <unistd.h>

int cnt = 0;
EventLoop* g_loop;

void printTid()
{
  printf("pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
  printf("now %s\n", Timestamp::now().toString().c_str());
}

void print(const char* msg)
{
  printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
  if (++cnt == 20)
  {
    g_loop->quit();
  }
}

void cancel(TimerId timer)
{
  g_loop->cancel(timer);
  printf("cancelled at %s\n", Timestamp::now().toString().c_str());
}

int main()
{
  printTid();
  sleep(1);
  {
    EventLoop loop;
    g_loop = &loop;

    print("main");
    loop.runAfter(1, std::bind(print, "once1"));
    loop.runAfter(1.5, std::bind(print, "once1.5"));
    loop.runAfter(2.5, std::bind(print, "once2.5"));
    loop.runAfter(3.5, std::bind(print, "once3.5"));
    TimerId t45 = loop.runAfter(4.5, std::bind(print, "once4.5"));
    loop.runAfter(4.2, std::bind(cancel, t45));
    loop.runAfter(4.8, std::bind(cancel, t45));
    loop.runEvery(2, std::bind(print, "every2"));
    TimerId t3 = loop.runEvery(3, std::bind(print, "every3"));
    loop.runAfter(9.001, std::bind(cancel, t3));

    loop.loop();
    print("main loop exits");
  }
  sleep(1);
  {
    EventLoopThread loopThread;
    EventLoop* loop = loopThread.startLoop();
    loop->runAfter(2, printTid);
    sleep(3);
    print("thread loop exits");
  }
}
