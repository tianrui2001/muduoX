#include "HttpServer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include <muduoX/AsyncLogging.h>
#include <muduoX/Logger.h>
#include <muduoX/MemoryPool.h> // 假设你的内存池头文件在这里
#include <muduoX/EventLoop.h>

#include <iostream>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// --- 异步日志的全局钩子 ---
// 因为 Logger::setOutput 需要一个函数指针，我们需要这个全局变量
AsyncLogging* g_asyncLog = nullptr;

void asyncOutput(const char* msg, int len)
{
    g_asyncLog->append(msg, len);
}

// --- 业务逻辑回调 ---
// 这里演示如何处理 API 请求，如果不是 API，则返回 404 (或者交还给 HttpServer 处理静态文件)
// 注意：根据你之前 HttpServer::onRequest 的逻辑，如果设置了 Callback，
// 且 Callback 填充了 Response，就不会去读静态文件了。
void onHttpRequest(const HttpRequest& req, HttpResponse* resp)
{
    // 打印请求日志（会走异步日志）
    LOG_INFO << "Headers " << req.methodString() << " " << req.path();

    // 简单的 API 路由
    if (req.path() == "/api/hello")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("application/json");
        resp->setBody("{\"message\": \"Hello from muduoX io_uring server!\", \"status\": 1}");
    }
    else if (req.path() == "/api/version")
    {
        resp->setStatusCode(HttpResponse::k200Ok);
        resp->setStatusMessage("OK");
        resp->setContentType("text/plain");
        resp->setBody("muduoX v1.0.0 with io_uring");
    }
    // 如果不是 API 请求，什么都不做，留给 HttpServer 的默认逻辑去处理静态文件
    // (前提是你的 HttpServer::onRequest 逻辑支持这种“透传”)
}

int main(int argc, char* argv[])
{
    // =======================================================
    // 1. 初始化内存池 (Memory Pool)
    // =======================================================
    // 这是非常有用的！它预先分配一大块内存。
    // 之后你的 TcpConnection 创建 HttpContext 时，如果使用了 memoryPool::newElement
    // 速度会非常快。
    HashBucket::initMemoryPools();
    // (这行简单的代码背后，是64个预分配的内存池在待命)


    // =======================================================
    // 2. 初始化异步日志 (Async Logging)
    // =======================================================
    // 设置滚动大小为 500MB
    AsyncLogging log("WebServer", 500 * 1024 * 1024);
    
    // 将 log 对象赋值给全局指针，供回调使用
    g_asyncLog = &log;
    
    // 【关键】接管系统默认的日志输出
    // 此后，项目中所有的 LOG_INFO/ERROR 都会流向 AsyncLogging 线程
    // 绝对不会阻塞你的 io_uring 线程
    Logger::setOutput(asyncOutput);
    
    // 启动日志后端线程
    log.start();

    LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
    LOG_INFO << "AsyncLogging started.";


    // =======================================================
    // 3. 准备静态资源目录
    // =======================================================
    std::string webRoot = "./www";
    struct stat st;
    if (::stat(webRoot.c_str(), &st) < 0) {
        // 如果 www 目录不存在，创建一个，方便测试
        LOG_WARN << "Web root ./www not found, creating a demo one...";
        if (::mkdir(webRoot.c_str(), 0755) == 0) {
            // 创建一个测试用的 index.html
            FILE* fp = ::fopen("./www/index.html", "w");
            if (fp) {
                fprintf(fp, "<html><head><title>MuduoX</title></head><body><h1>Welcome to MuduoX io_uring Server!</h1></body></html>");
                ::fclose(fp);
            }
        }
    }


    // =======================================================
    // 4. 启动 HTTP 服务器
    // =======================================================
    EventLoop loop;
    InetAddress listenAddr(8000);
    
    // 创建服务器，指定静态文件根目录
    HttpServer server(&loop, listenAddr, "MuduoX-Http", webRoot);

    // 设置业务回调（可选，如果不设置，就变成纯静态文件服务器）
    server.setHttpCallback(onHttpRequest);

    // 设置线程数量 (io_uring 将在每个线程中独立工作)
    // 建议设置为 CPU 核心数
    server.setThreadNum(4); 

    LOG_INFO << "HttpServer starting on port 8000 with io_uring backend...";
    
    server.start(); // 启动监听，创建线程池

    loop.loop();    // 主线程进入事件循环

    return 0;
}