#include <muduoX/TcpServer.h>
#include <muduoX/EventLoop.h>
#include <string>
#include <functional>

class HttpRequest;
class HttpResponse;
class UringManager; // 前向声明

class HttpServer
{
public:
    using HttpCallback = std::function<void(const HttpRequest&, HttpResponse*)>;

    HttpServer(EventLoop* loop,
               const InetAddress& listenAddr,
               const std::string& name,
               const std::string& webRoot = "./www"); // 默认静态资源目录

    void start();

    // 设置工作线程数
    void setThreadNum(int numThreads) { server_.setThreadNum(numThreads); }

    // (可选) 如果用户想覆盖默认的静态文件处理逻辑，可以设置这个回调
    // 如果不设置，默认执行静态文件服务
    void setHttpCallback(const HttpCallback& cb) { httpCallback_ = cb; }

private:
    void onConnection(const TcpConnectionPtr& conn);
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime);
    void onRequest(const TcpConnectionPtr& conn, const HttpRequest& req);

    // 具体的静态文件处理逻辑
    void handleStaticFile(const TcpConnectionPtr& conn, const HttpRequest& req, HttpResponse& resp);
    // 具体的错误处理逻辑
    void handleError(const TcpConnectionPtr& conn, HttpResponse& resp, int statusCode, const std::string& msg);

    TcpServer server_;
    EventLoop* loop_;
    std::string webRoot_; // 静态文件根目录
    HttpCallback httpCallback_;
};
