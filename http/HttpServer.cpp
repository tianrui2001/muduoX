#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "HttpServer.h"
#include "HttpData.h"       // 你之前的 MimeType 类
#include <muduoX/IOuring.h> // 包含你的 Uring 定义
#include <muduoX/Logger.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>


HttpServer::HttpServer(EventLoop* loop,
                       const InetAddress& listenAddr,
                       const std::string& name,
                       const std::string& webRoot)
    : server_(loop, listenAddr, name),
      loop_(loop),
      webRoot_(webRoot)
{
    server_.setConnectionCallback(
        std::bind(&HttpServer::onConnection, this, std::placeholders::_1));
    server_.setMessageCallback(
        std::bind(&HttpServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void HttpServer::start()
{
    server_.start();
}

void HttpServer::onConnection(const TcpConnectionPtr& conn)
{
    if (conn->connected()) {
        // 绑定 HttpContext 到 Connection
        conn->setContext(HttpContext());
    }
}

void HttpServer::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp receiveTime)
{
    // 1. 取出上下文
    HttpContext* context = std::any_cast<HttpContext>(conn->getMutableContext());

    while(buf->readableBytes() > 0){
        // 2. 状态机解析 (调用你移植的解析逻辑)
        if (!context->parseRequest(buf, receiveTime)) {
            conn->send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->shutdown();
        }

        // 3. 解析完成，进入业务逻辑
        if (context->gotAll()) {
            LOG_DEBUG << "HttpServer::onMessage - Request Complete";
            onRequest(conn, context->request());
            context->reset(); // 重置状态机，准备处理 Keep-Alive 的下一个请求
        } else {
            break; // 继续等待更多数据
        }
    }
    
}

void HttpServer::onRequest(const TcpConnectionPtr& conn, const HttpRequest& req)
{
    // 1. 处理 HTTP 长连接头 (LinYa 逻辑)
    const std::string& connection = req.getHeader("Connection");
    bool close = (connection == "close") || 
                 (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");

    HttpResponse response(close);

    // 2. 尝试执行业务回调
    if (httpCallback_) {
        httpCallback_(req, &response);
    }

    // 3. 【核心修改】检查回调是否处理了该请求
    // 如果状态码不是 Unknown (0)，说明回调已经处理了，直接发送
    if (response.statusCode() != HttpResponse::kUnknown) {
        auto buf = std::make_shared<Buffer>();
        response.appendToBuffer(buf.get());
        conn->getLoop()->runInLoop([conn, buf, response](){
            conn->send(buf.get());
        });
        if (response.closeConnection()) {
            conn->shutdown();
        }
        return; 
    }

    // 4. 默认逻辑：处理静态文件 (io_uring 登场)
    handleStaticFile(conn, req, response);
}

void HttpServer::handleStaticFile(const TcpConnectionPtr& conn, 
                                    const HttpRequest& req, 
                                    HttpResponse& response)
{
    // 1. 解析路径
    std::string path = req.path();
    if (path == "/") path = "/index.html";
    
    // 安全检查：防止访问上一级目录 (简单实现)
    if (path.find("..") != std::string::npos) {
        handleError(conn, response, 403, "Forbidden");
        return;
    }

    std::string filepath = webRoot_ + path;

    // 2. 检查文件是否存在 (同步 stat 很快，通常不影响，除非磁盘极其繁忙)
    // 极致优化可以用 io_uring_prep_statx，但这里先用 stat
    struct stat st;
    if (::stat(filepath.c_str(), &st) < 0) {
        handleError(conn, response, 404, "Not Found");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        handleError(conn, response, 404, "Not Found (Is Directory)");
        return;
    }

    // 1. 提前记录好业务需要的信息，不要依赖外部 response 对象的引用
    const std::string& connection = req.getHeader("Connection");
    bool shouldClose = (connection == "close") || 
                       (req.getVersion() == HttpRequest::kHttp10 && connection != "Keep-Alive");
    
    // 获取后缀名
    size_t dot_pos = path.find_last_of('.');
    std::string suffix;
    if (dot_pos != std::string::npos) suffix = path.substr(dot_pos);
    std::string contentType = MimeType::getMime(suffix); 

    // 4. 【核心亮点】使用 io_uring 异步读取文件
    UringManager* uring = loop_->getUringManager();
    if (!uring) {
        // 如果当前 loop 没有配置 io_uring，回退到同步读取或报错
        // 这里为了演示，假设必须有
        LOG_ERROR << "No UringManager found in EventLoop";
        return;
    }

    // 打开文件 (这里也是异步的/非阻塞的逻辑封装)
    // 注意：openFile 内部会创建 File 对象并返回 shared_ptr
    // O_RDONLY 即可
    auto file = uring->registerFile(filepath);
    if (!file) {
        handleError(conn, response, 404, "Open Failed");
        return;
    }

    // 准备读取缓冲区
    // 注意：缓冲区必须在异步操作期间存活。
    // 我们使用 shared_ptr<vector> 来管理这块内存的生命周期，并将其捕获进 lambda
    size_t fileSize = st.st_size;
    auto readBuffer = std::make_shared<std::vector<char>>(fileSize);

    struct iovec iov = {
        .iov_base = readBuffer->data(),
        .iov_len = fileSize
    };
    RWOperation op(RWOperation::READ, 0, iov); // 从偏移量 0 开始读

    // 发起异步读
    // 注意 lambda 捕获了 conn, response (拷贝), readBuffer, file (保持打开状态)
    // 注意 lambda 捕获了 conn, response (拷贝), readBuffer, file (保持打开状态)
     // 3. 发起异步读，只捕获必要的基础数据
    file->asyncRW(std::move(op), [conn, shouldClose, contentType, readBuffer, file](int res, RWOperation& op_back) {
        if (res < 0) {
            LOG_ERROR << "Async read failed";
            conn->shutdown();
            return;
        }

        // 4. 【核心改动】在回调内部现场创建 Response 对象
        // 这样可以确保这个对象是 100% 完整且安全的
        HttpResponse response(shouldClose); 
        response.setStatusCode(HttpResponse::k200Ok);
        response.setStatusMessage("OK");
        response.setContentType(contentType);
        response.addHeader("Server", "muduoX-io_uring");
        
        // 填入读到的内容
        response.setBody(std::string(readBuffer->data(), res));

        // 5. 序列化并发送
        Buffer outputBuf;
        response.appendToBuffer(&outputBuf);
         
        // 使用只能指针来解决发送数据时内存失效的问题
        auto sendDataPtr = std::make_shared<std::string>(outputBuf.retrieveAllAsString());

        LOG_INFO << "Callback sending data, size: " << sendDataPtr->size();

        // 4. 【关键】：在 runInLoop 中捕获这个 shared_ptr
        // 这样即使当前这个 io_uring 的 Lambda 结束了，
        // sendDataPtr 指向的内存依然被 runInLoop 里的 Lambda 持有，直到发送完成
        conn->getLoop()->runInLoop([conn, sendDataPtr, shouldClose]() {
            // 在这里调用你现有的 send(const std::string&)
            // 此时 sendDataPtr->c_str() 指向的内存是绝对安全的
            conn->send(*sendDataPtr);

            if (shouldClose) {
                conn->shutdown();
            }
        });

        
        LOG_INFO << "File sent via io_uring: " << res << " bytes";
    });
}

void HttpServer::handleError(const TcpConnectionPtr& conn, HttpResponse& response, int statusCode, const std::string& msg)
{
    response.setStatusCode((HttpResponse::HttpStatusCode)statusCode);
    response.setStatusMessage(msg);
    response.setBody("<html><body><h1>" + std::to_string(statusCode) + " " + msg + "</h1></body></html>");
    response.setCloseConnection(true);
    
    auto buf = std::make_shared<Buffer>();
    response.appendToBuffer(buf.get());
    conn->getLoop()->runInLoop([conn, buf, response](){
            conn->send(buf.get());
        });
    conn->shutdown();
}