#pragma once

#include <muduoX/Buffer.h>
#include <muduoX/TimeStamp.h>

#include "HttpRequest.h"

class HttpContext
{
public:
    // 主状态机 ：定义了处理一个完整 HTTP 请求的主要阶段
    enum ProcessState {
        STATE_PARSE_URI = 1,
        STATE_PARSE_HEADERS,
        STATE_RECV_BODY,
        STATE_ANALYSIS,
        STATE_FINISH
    };

    // 子任务返回码 ：解析 URI
    enum URIState {
        PARSE_URI_AGAIN = 1,
        PARSE_URI_ERROR,
        PARSE_URI_SUCCESS,
    };

    // 子任务返回码 ：解析 Headers
    enum HeaderState {
        PARSE_HEADER_SUCCESS = 1,
        PARSE_HEADER_AGAIN,
        PARSE_HEADER_ERROR
    };

    // parseHeaders 函数内部使用的、一个更低层次的字符级状态机
    enum ParseState {
        H_START = 0,
        H_KEY,
        H_COLON,
        H_SPACES_AFTER_COLON,
        H_VALUE,
        H_CR,
        H_LF,
        H_END_CR,
        H_END_LF
    };

    // --- 构造函数 ---
    HttpContext()
        : state_(STATE_PARSE_URI),
          hState_(H_START)  {}

    // --- 核心接口 ---
    
    // 替代了原 HttpData::handleRead 中的 switch-case 逻辑
    bool parseRequest(Buffer* buf, Timestamp receiveTime);

    bool gotAll() const { return state_ == STATE_FINISH; }

    void reset() {
        state_ = STATE_PARSE_URI;
        hState_ = H_START;
        HttpRequest dummy;
        request_.swap(dummy);
    }

    const HttpRequest& request() const { return request_; }
    HttpRequest& request() { return request_; }


private:
    // --- 内部解析函数 (移植自 LinYa) ---
    URIState parseURI(Buffer* buf);
    HeaderState parseHeaders(Buffer* buf);
    
    // --- 成员变量 ---
    ProcessState state_;
    ParseState hState_; // 头部解析的微观状态
    
    HttpRequest request_; // 替代了 HttpData 中零散的 method_, path_, headers_ 等变量
};