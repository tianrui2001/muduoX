#pragma once

#include <muduoX/TimeStamp.h>
#include <string>
#include <map>
#include <stdio.h>

class HttpRequest {
public:
    // 1. 定义枚举：将 LinYa 的全局枚举封装在类内部，防止污染全局命名空间
    enum Method {
        kInvalid, kGet, kPost, kHead, kPut, kDelete
    };

    enum Version {
        kUnknown, kHttp10, kHttp11
    };

    HttpRequest()
        : method_(kInvalid),
          version_(kUnknown) {}

    // =======================================================
    // Setters (供 HttpContext 在解析状态机中调用)
    // =======================================================

    // 设置版本 (对应 LinYa parseURI 中的 HTTP 版本判断)
    void setVersion(Version v) { version_ = v; }

    // 返回 bool 表示方法是否合法
    bool setMethod(const char* start, const char* end) {
        std::string m(start, end);  // 获取子字符串
        if (m == "GET") {
            method_ = kGet;
        } else if (m == "POST") {
            method_ = kPost;
        } else if (m == "HEAD") {
            method_ = kHead;
        } else if (m == "PUT") {
            method_ = kPut;
        } else if (m == "DELETE") {
            method_ = kDelete;
        } else {
            method_ = kInvalid;
        }
        return method_ != kInvalid;
    }

    // 设置路径 (对应 parseURI 中的 fileName_ / path_)
    void setPath(const char* start, const char* end) { path_.assign(start, end); }

    // 设置查询参数 (对应 URL 中 ? 后面的内容)
    void setQuery(const char* start, const char* end) { query_.assign(start, end); }

    // 设置接收时间 (用于日志或性能统计)
    void setReceiveTime(Timestamp t) { receiveTime_ = t; }

    // 设置 Body (对应 LinYa STATE_RECV_BODY 阶段)
    void setBody(const std::string& body) { body_ = body; }

    Version getVersion() const { return version_; }

    // 添加头部 ： headers_[field] = value 。优化点：自动处理冒号后的空格
    void addHeader(const char* start, const char* colon, const char* end) {
        std::string field(start, colon); // Key
        ++colon;
        // 跳过 Value 前面的空格 (Trim)
        while (colon < end && isspace(*colon)) {
            ++colon;
        }
        std::string value(colon, end);   // Value
        
        // LinYa 代码是 headers_[key] = value;
        while (!value.empty() && isspace(value[value.size() - 1])) {
            value.resize(value.size() - 1);
        }
        headers_[field] = value;
    }

    // =======================================================
    // Getters (供 HttpServer::onRequest 业务逻辑调用)
    // =======================================================

    Method method() const { return method_; }
    
    // 辅助函数：将枚举转回字符串 (用于日志)
    const char* methodString() const {
        const char* result = "UNKNOWN";
        switch (method_) {
            case kGet: result = "GET"; break;
            case kPost: result = "POST"; break;
            case kHead: result = "HEAD"; break;
            case kPut: result = "PUT"; break;
            case kDelete: result = "DELETE"; break;
            default: break;
        }
        return result;
    }

    const std::string& path() const { return path_; }
    const std::string& query() const { return query_; }
    Timestamp receiveTime() const { return receiveTime_; }

    // 获取特定 Header，不存在返回空字符串
    std::string getHeader(const std::string& field) const {
        std::string result;
        auto it = headers_.find(field);
        if (it != headers_.end()) {
            result = it->second;
        }
        return result;
    }

    const std::map<std::string, std::string>& headers() const { return headers_; }
    const std::string& body() const { return body_; }

    // =======================================================
    // 工具函数
    // =======================================================

    // 高效重置对象 (用于 Keep-Alive，替代 reset 函数)
    void swap(HttpRequest& that) {
        std::swap(method_, that.method_);
        std::swap(version_, that.version_);
        path_.swap(that.path_);
        query_.swap(that.query_);
        // 如果你的 Timestamp 没有 swap，就用 std::swap 或者直接赋值
        // std::swap(receiveTime_, that.receiveTime_); 
        receiveTime_ = that.receiveTime_; 
        headers_.swap(that.headers_);
        body_.swap(that.body_);
    }

private:
    Method method_;
    Version version_;
    std::string path_;
    std::string query_;
    Timestamp receiveTime_;
    std::map<std::string, std::string> headers_;
    std::string body_;
};
