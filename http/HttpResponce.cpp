#include "HttpResponse.h"

#include <muduoX/Buffer.h> // 必须包含 Buffer 的完整定义
#include <stdio.h>
#include <string.h>

void HttpResponse::appendToBuffer(Buffer* output) const {
    char buf[32];

    // 1. 响应行: "HTTP/1.1 200 OK\r\n"
    // LinYa: header += "HTTP/1.1 200 OK\r\n";
    snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
    output->append(buf, strlen(buf));
    output->append(statusMessage_.c_str(), statusMessage_.size());
    output->append("\r\n", 2);

    // 2. 处理 Connection 和 Content-Length
    // LinYa 的代码中手动处理了 Keep-Alive 和 Content-Length
    // 这里我们自动处理，防止业务逻辑忘记写
    if (closeConnection_) {
        // 如果是非 Keep-Alive，通知客户端关闭
        const char* connClose = "Connection: close\r\n";
        output->append(connClose, strlen(connClose));
    } else {
        // HTTP/1.1 默认 Keep-Alive，但显式发送更稳妥
        // 且 Keep-Alive 连接必须有 Content-Length (或 Chunked 编码，这里暂不支持 Chunked)
        snprintf(buf, sizeof(buf), "Content-Length: %zd\r\n", body_.size());
        output->append(buf, strlen(buf));

        const char* connKeep = "Connection: Keep-Alive\r\n";
        output->append(connKeep, strlen(connKeep));
    }

    // 3. 处理其他头部 (Content-Type, Server 等)
    // 遍历 map，将 headers 写入 buffer
    for (const auto& header : headers_) {
        output->append(header.first.c_str(), header.first.size());
        output->append(": ", 2);
        output->append(header.second.c_str(), header.second.size());
        output->append("\r\n", 2);
    }

    // 4. 空行 (Header 结束标志)
    output->append("\r\n", 2);

    // 5. 响应体 (Body)
    // LinYa: outBuffer_ += ... string(src_addr, ...)
    output->append(body_.c_str(), body_.size());
}