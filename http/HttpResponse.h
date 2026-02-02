#pragma once

#include <map>
#include <string>
#include <vector>

class Buffer; // 前向声明，减少头文件依赖

class HttpResponse {
public:
    enum HttpStatusCode {
        kUnknown,
        k200Ok = 200,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k404NotFound = 404,
        k500InternalServerError = 500,
    };

    // 构造函数
    // close: 根据请求头中的 Connection 字段决定默认是否关闭连接
    explicit HttpResponse(bool close)
        : statusCode_(kUnknown),
          closeConnection_(close) {
    }

    // =======================================================
    // Setters (供 HttpServer::onRequest 业务逻辑调用)
    // =======================================================

    void setStatusCode(HttpStatusCode code) {
        statusCode_ = code;
    }

    void setStatusMessage(const std::string& message) {
        statusMessage_ = message;
    }

    void setCloseConnection(bool on) {
        closeConnection_ = on;
    }

    bool closeConnection() const {
        return closeConnection_;
    }

    // 设置 Content-Type (例如 "text/html", "image/jpeg")
    // 这里的值通常来自 MimeType::getMime()
    void setContentType(const std::string& contentType) {
        addHeader("Content-Type", contentType);
    }

    // 添加自定义头部 (例如 "Server: LinYa's Web Server")
    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }

    // 设置响应体 (Body)
    // 对应 LinYa 代码中读取文件内容或者生成错误页面的 HTML
    void setBody(const std::string& body) {
        body_ = body;
    }

    HttpStatusCode statusCode() const {
        return statusCode_;
    }

    // =======================================================
    // 核心接口：序列化
    // =======================================================

    // 将当前 Response 对象转换为 HTTP 协议格式的字节流，写入 Buffer
    void appendToBuffer(Buffer* output) const;

private:
    std::map<std::string, std::string> headers_; // 响应头
    HttpStatusCode statusCode_;                  // 状态码
    std::string statusMessage_;                  // 状态描述 (Reason Phrase)
    bool closeConnection_;                       // 是否需要在发送后关闭连接
    std::string body_;                           // 响应体数据
};
