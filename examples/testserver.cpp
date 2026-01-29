#include <muduoX/TcpServer.h>
#include <muduoX/Logger.h>
#include <string>
#include <functional>


class EchoService{
public:
    EchoService(EventLoop *loop,
                const InetAddress &listenAddr,
                const std::string &name)
        :loop_(loop),
        server_(loop, listenAddr, name)
    {
        server_.setConnectionCallback(
            std::bind(&EchoService::onConnection, this, std::placeholders::_1)
        );
        server_.setMessageCallback(
            std::bind(&EchoService::onMessage, this,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      std::placeholders::_3)
        );

        server_.setThreadNum(4); // 设置底层subLoop的数量
    }

    void start(){
        server_.start();
    }


private:
    // 连接回调函数
    void onConnection(const TcpConnectionPtr &conn){
        if(conn->connected()){
            LOG_INFO << "EchoService - " << conn->peerAddr().toIpPort().c_str() 
            << " -> " << conn->locallAddr().toIpPort().c_str() << " is online";
        } else {
            LOG_INFO << "EchoService - " << conn->peerAddr().toIpPort().c_str() 
            << " -> " << conn->locallAddr().toIpPort().c_str() << " is offline";
        }
    }

    // 消息回调函数
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp timestamp){
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();   // 关闭连接，发送完数据后关闭连接
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main(){
    EventLoop loop;
    InetAddress addr(8000);
    EchoService echoService(&loop, addr, "EchoService");
    echoService.start();
    loop.loop();
}


// #include <muduoX/EventLoop.h>
// #include <muduoX/EventLoopThread.h>
// #include <muduoX/Thread.h>

// #include <stdio.h>
// #include <unistd.h>

// int cnt = 0;
// EventLoop* g_loop;

// void printTid()
// {
//   printf("pid = %d, tid = %d\n", getpid(), CurrentThread::tid());
//   printf("now %s\n", Timestamp::now().toString().c_str());
// }

// void print(const char* msg)
// {
//   printf("msg %s %s\n", Timestamp::now().toString().c_str(), msg);
//   if (++cnt == 20)
//   {
//     g_loop->quit();
//   }
// }

// void cancel(TimerId timer)
// {
//   g_loop->cancel(timer);
//   printf("cancelled at %s\n", Timestamp::now().toString().c_str());
// }

// int main()
// {
//   printTid();
//   sleep(1);
//   {
//     EventLoop loop;
//     g_loop = &loop;

//     print("main");
//     loop.runAfter(1, std::bind(print, "once1"));
//     loop.runAfter(1.5, std::bind(print, "once1.5"));
//     loop.runAfter(2.5, std::bind(print, "once2.5"));
//     loop.runAfter(3.5, std::bind(print, "once3.5"));
//     TimerId t45 = loop.runAfter(4.5, std::bind(print, "once4.5"));
//     loop.runAfter(4.2, std::bind(cancel, t45));
//     loop.runAfter(4.8, std::bind(cancel, t45));
//     loop.runEvery(2, std::bind(print, "every2"));
//     TimerId t3 = loop.runEvery(3, std::bind(print, "every3"));
//     loop.runAfter(9.001, std::bind(cancel, t3));

//     loop.loop();
//     print("main loop exits");
//   }
//   sleep(1);
//   {
//     EventLoopThread loopThread;
//     EventLoop* loop = loopThread.startLoop();
//     loop->runAfter(2, printTid);
//     sleep(3);
//     print("thread loop exits");
//   }
// }



// #include <muduoX/EventLoop.h>
// #include <muduoX/IOuring.h>
// #include <muduoX/Logger.h>

// #include <iostream>
// #include <string>
// #include <vector>
// #include <unistd.h> // for ::close
// #include <fcntl.h>  // for O_RDWR etc.
// #include <cstring>  // for strlen

// int main()
// {
//     LOG_INFO << "Starting Uring Test...\n";

//     // 1. 创建 EventLoop，它会自动创建 UringManager
//     EventLoop loop;
//     UringManager* uringManager = loop.getUringManager();
//     if (!uringManager) {
//         LOG_FATAL << "Failed to get UringManager from EventLoop!\n";
//         return 1;
//     }

//     // --- 准备工作 ---
//     const std::string filename = "uring_test_file.txt";
//     std::string write_content = "Hello, this is a test for my muduoX with io_uring!";
//     std::vector<char> read_buffer(1024, 0); // 准备一个足够大的读缓冲区

//     // 2. 使用 UringManager 打开文件，获取一个智能指针File句柄
//     //    O_RDWR: 读写模式
//     //    O_CREAT: 如果文件不存在则创建
//     //    O_TRUNC: 如果文件已存在则清空内容
//     std::shared_ptr<File> file = uringManager->registerFile(filename);
    
//     if (!file) {
//         LOG_FATAL << "Failed to register file: " << filename << "\n";
//         return 1;
//     }
    
//     // 3. 准备第一个I/O操作：异步写入
//     struct iovec write_iov = {
//         .iov_base = (void*)write_content.c_str(),
//         .iov_len = write_content.length()
//     };
//     // 创建一个写操作描述，从文件偏移量 0 开始
//     RWOperation write_op(RWOperation::WRITE, 0, write_iov);

//     LOG_INFO << "Submitting async write request...\n";

//     // 4. 发起异步写请求，并提供一个 lambda 作为回调函数
//     file->asynRW(
//         std::move(write_op),
//         // 回调函数，捕获了所有需要的上下文信息
//         [&](int write_res, const RWOperation& completed_op) {
//             if (write_res < 0) {
//                 LOG_ERROR << "Async write failed: " << strerror(-write_res) << "\n";
//                 loop.quit();
//                 return;
//             }

//             LOG_INFO << "Async write completed successfully, bytes written: " << write_res << "\n";

//             // --- 在写的回调中，发起异步读请求 ---
            
//             // 5. 准备读操作
//             struct iovec read_iov = {
//                 .iov_base = read_buffer.data(),
//                 .iov_len = read_buffer.size()
//             };
//             // 创建一个读操作描述，同样从文件偏移量 0 开始
//             RWOperation read_op(RWOperation::READ, 0, read_iov);
            
//             LOG_INFO << "Submitting async read request in write callback...\n";
            
//             // 6. 发起异步读请求
//             file->asynRW(
//                 std::move(read_op),
//                 [&](int read_res, const RWOperation& completed_read_op) {
//                     if (read_res < 0) {
//                         LOG_ERROR << "Async read failed: " << strerror(-read_res) << "\n";
//                     } else {
//                         LOG_INFO << "Async read completed successfully, bytes read: " << read_res << "\n";
//                         std::cout << "--- File Content ---" << std::endl;
//                         std::cout << std::string(read_buffer.data(), read_res) << std::endl;
//                         std::cout << "--------------------" << std::endl;
//                     }

//                     // 7. 所有操作完成，退出事件循环
//                     LOG_INFO << "All I/O finished, quitting loop.\n";
//                     loop.quit();
//                 }
//             );
//         }
//     );

//     // 8. 启动事件循环，等待所有异步操作完成
//     loop.loop();

//     LOG_INFO << "Uring Test finished.\n";
//     // file 对象在这里的 shared_ptr 析构，如果所有异步操作已完成，文件将被关闭。
    
//     return 0;
// }