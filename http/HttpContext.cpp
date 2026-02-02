#include "HttpContext.h"

#include <algorithm>
#include <muduoX/Buffer.h>
#include <muduoX/Logger.h>


// 核心驱动循环 (对应原 HttpData::handleRead 的逻辑)
bool HttpContext::parseRequest(Buffer* buf, Timestamp receiveTime) {
    bool ok = true;
    bool hasMore = true;

    while (hasMore) {
        // 1. 解析请求行 (URI)
        if (state_ == STATE_PARSE_URI) {
            URIState flag = parseURI(buf);
            if (flag == PARSE_URI_AGAIN) {
                hasMore = false; // 数据不够，等下次
            } else if (flag == PARSE_URI_ERROR) {
                hasMore = false;
                ok = false;      // 格式错误
            } else {
                // 成功，设置时间，状态流转
                request_.setReceiveTime(receiveTime);
                state_ = STATE_PARSE_HEADERS;
            }
        } 
        // 2. 解析头部 (Headers)
        else if (state_ == STATE_PARSE_HEADERS) {
            HeaderState flag = parseHeaders(buf);
            if (flag == PARSE_HEADER_AGAIN) {
                hasMore = false;
            } else if (flag == PARSE_HEADER_ERROR) {
                hasMore = false;
                ok = false;
            } else {
                // 头部解析完成
                if (request_.method() == HttpRequest::kPost) {
                    state_ = STATE_RECV_BODY;
                } else {
                    state_ = STATE_ANALYSIS; // 其实就是 Finish
                }
            }
        } 
        // 3. 接收 Body (POST)
        else if (state_ == STATE_RECV_BODY) {
            // 这里简单移植：检查 Content-Length
            // 注意：需要你在 HttpRequest 中实现 hasHeader 和 getHeader
            if (request_.headers().count("Content-Length")) {
                size_t len = std::stoul(request_.headers().at("Content-Length"));
                if (buf->readableBytes() >= len) {
                    request_.setBody(std::string(buf->peek(), len));
                    buf->retrieve(len); // 吃掉 Body
                    state_ = STATE_ANALYSIS;
                } else {
                    hasMore = false; // 还没收齐
                }
            } else {
                // 没有 Content-Length 的 POST？当作错误或空 body
                state_ = STATE_ANALYSIS; 
            }
        }
        // 4. 解析完成
        else if (state_ == STATE_ANALYSIS) {
             state_ = STATE_FINISH;
             hasMore = false;
        }
    }
    return ok;
}

// --- 移植 parseURI ---
// 原逻辑：查找 \r，截取 substr，查找 GET/POST...
// 新逻辑：操作 Buffer 指针，设置 HttpRequest
HttpContext::URIState HttpContext::parseURI(Buffer* buf) {
    if (buf->readableBytes() == 0) return PARSE_URI_AGAIN;

    const char* begin = buf->peek();
    const char* end = begin + buf->readableBytes();
    
    // 查找请求行的结束位置 \r\n
    const char* crlf = std::find(begin, end, '\r');
    if (crlf == end) {
        return PARSE_URI_AGAIN; // 没收齐一行
    }

    // 这行就是请求行
    // request_line 范围: [begin, crlf)
    
    // 1. 解析 Method
    const char* space = std::find(begin, crlf, ' ');
    if (space == crlf) return PARSE_URI_ERROR;
    
    // HttpRequest::setMethod 会处理字符串到枚举的转换
    if (!request_.setMethod(begin, space)) {
        return PARSE_URI_ERROR;
    }

    // 2. 解析 Path (Filename)
    const char* space2 = std::find(space + 1, crlf, ' ');
    if (space2 == crlf) return PARSE_URI_ERROR;
    
    const char* question = std::find(space + 1, space2, '?');
    if (question != space2) {
        request_.setPath(space + 1, question);
        request_.setQuery(question + 1, space2);
    } else {
        request_.setPath(space + 1, space2);
    }

    // 3. 解析 Version
    // LinYa 的代码是 substr(pos+1, 3) == "1.0"
    const char* verStart = space2 + 1;
    std::string ver(verStart, crlf);
    if (ver == "HTTP/1.0") {
        request_.setVersion(HttpRequest::kHttp10);
    } else if (ver == "HTTP/1.1") {
        request_.setVersion(HttpRequest::kHttp11);
    } else {
        return PARSE_URI_ERROR;
    }

    // 【重要】从 Buffer 中移除已解析的请求行 (包括 \r\n)
    // LinYa 代码是 string.substr，这里对应 retrieve
    // 假设 crlf 指向 \r，那么下一位应该是 \n
    if (*(crlf + 1) == '\n') {
        buf->retrieveUntil(crlf + 2);
    } else {
        return PARSE_URI_ERROR; // 只有 \r 没有 \n？
    }

    return PARSE_URI_SUCCESS;
}

// --- 移植 parseHeaders ---
// 这是 LinYa 代码中最复杂的微观状态机。
// 我们保持他的 switch-case 结构，但把 string 索引换成指针操作。
HttpContext::HeaderState HttpContext::parseHeaders(Buffer* buf) {
    if (buf->readableBytes() == 0) return PARSE_HEADER_AGAIN;

    const char* current = buf->peek();
    const char* end = current + buf->readableBytes();
    const char* start = current; // 记录本次解析的起点，用于 retrieve
    
    // 临时变量用于存储解析中的 key/value 位置
    // 因为要跨越多次循环，这里简化处理：假设一次 parseHeaders 调用内能解析完一个 KV
    // 如果想要完全还原 LinYa 跨调用的能力，需要把这些变量变成成员变量
    // 但通常 Buffer 处理速度很快，我们这里用局部变量配合 Buffer 指针
    const char* key_start = nullptr;
    const char* key_end = nullptr;
    const char* val_start = nullptr;
    const char* val_end = nullptr;

    // 我们需要知道当前处理了多少字节，以便最后 retrieve
    // 这里的逻辑稍微修改一下：我们逐个字符推进，一旦形成完整的 Key-Value，就存入 Request
    
    while (current < end) {
        char ch = *current;
        switch (hState_) {
            case H_START:
                if (ch == '\n' || ch == '\r') break; // 跳过空行
                hState_ = H_KEY;
                key_start = current;
                LOG_INFO << "Header Key Start at: " << std::string(key_start, end);
                break;

            case H_KEY:
                if (ch == ':') {
                    key_end = current;
                    hState_ = H_COLON;
                    LOG_INFO << "Header Key End at: " << std::string(key_start, key_end);
                } else if (ch == '\n' || ch == '\r') {
                    return PARSE_HEADER_ERROR;
                }
                break;

            case H_COLON:
                if (ch == ' ') {
                    hState_ = H_SPACES_AFTER_COLON;
                    LOG_INFO << "Header Colon at: " << std::string(key_end, current + 1);
                } else {
                    return PARSE_HEADER_ERROR;
                }
                break;

            case H_SPACES_AFTER_COLON:
                hState_ = H_VALUE;
                val_start = current;
                LOG_INFO << "Header Value Start at: " << std::string(val_start, end);
                break;

            case H_VALUE:
                if (ch == '\r') {
                    hState_ = H_CR;
                    val_end = current;
                    LOG_INFO << "Header Value End at: " << std::string(val_start, val_end);
                } else if (ch == '\n') {
                    // 容错：直接遇到 \n
                    hState_ = H_LF;
                    val_end = current;
                    // 保存 Header
                    request_.addHeader(key_start, key_end, val_end);
                    LOG_INFO << "Header Value End at: " << std::string(val_start, val_end);
                }
                break;

            case H_CR:
                if (ch == '\n') {
                    hState_ = H_LF;
                    request_.addHeader(key_start, key_end, val_end);
                    LOG_INFO << "Header Value End at: " << std::string(val_start, val_end);
                } else {
                    return PARSE_HEADER_ERROR;
                }
                break;

            case H_LF:
                if (ch == '\r') {
                    hState_ = H_END_CR;
                    LOG_INFO << "Header End CR at: " << std::string(current, current + 1);
                } else {
                    key_start = current;
                    hState_ = H_KEY;
                    LOG_INFO << "Header Key Start at: " << std::string(key_start, end);
                }
                break;

            case H_END_CR:
                if (ch == '\n') {
                    hState_ = H_END_LF;
                    LOG_INFO << "Header End LF at: " << std::string(current, current + 1);

                    // 【关键修改】在这里直接计算并返回，不要等下一次循环
                    buf->retrieve(current - start + 1); 
                    LOG_INFO << "Buffer retrieved up to end of headers.";
                    return PARSE_HEADER_SUCCESS; 
                } else {
                    return PARSE_HEADER_ERROR;
                }
                break;
            
            // case H_END_LF:
            //     // 头部结束
            //     // 此时 current 指向 \n，我们应该把之前所有处理过的都 retrieve 掉
            //     buf->retrieve(current - start + 1); 
            //     LOG_INFO << "Buffer retrieved up to: " << std::string(start, current + 1);
            //     return PARSE_HEADER_SUCCESS;
        }
        current++;
    }

    return PARSE_HEADER_AGAIN;
}