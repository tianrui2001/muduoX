#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>


/**
//布局：
// +-------------------+------------------+------------------+
// | prependable bytes |  readable bytes  |  writable bytes  |
// +-------------------+------------------+------------------+
// |                   |                  |                  |
// 0      <=      readerIndex_   <=   writerIndex_    <=     size()
 */
class Buffer {
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initSize = kInitialSize)
        :buffer_(kCheapPrepend + initSize),
            readerIndex_(kCheapPrepend),
            writerIndex_(kCheapPrepend){}

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    // 返回可读数据的起始地址
    const char* peek() const { return begin() +readerIndex_; }

    // 复位可读数据的位置
    void retrieve(size_t len){
        if(len < readableBytes()){  // 只读走了一部分数据
            readerIndex_ += len;
        } else {
            retrieveAll();
        }
    }

    void retrieveAll(){
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    // 把onMessage回调函数上报的buffer数据全部读出，转换成string返回
    std::string retrieveAllAsString() {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len) {
        // 可读数据的地址
        std::string result(peek(), len);
        retrieve(len);
        return result;
    }

    void ensureWritableBytes(size_t len){
        if(writableBytes() < len){
            makeSpace(len);
        }
    }

    // 把[data, data+len]内存上的数据添加到writable缓冲区当中
    void append(const char* data, size_t len){
        ensureWritableBytes(len);
        // 可写数据的地址
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite() { return begin() + writerIndex_; }
    const char* beginWrite() const { return begin() + writerIndex_; }

    ssize_t readFd(int fd, int* saveErrno);     // 从fd上读取数据
    ssize_t writeFd(int fd, int* saveErrno);    // 通过fd发送数据

private:
    char* begin() { return &*buffer_.begin(); } // 返回vector底层数组的起始地址
    const char* begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len){
        if(writableBytes() + prependableBytes() < len + kCheapPrepend){
            // 没写的+读完的空间 < 需要写的 + 8字节的预留空间
            buffer_.resize(writerIndex_ + len);
        } else {
            // 将没有读完的数据移动到前面去， 后面填充write
            size_t readable = readableBytes();
            // copy(源头_开始, 源头_结束, 目标_开始)
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;    // 可读数据的起始位置
    size_t writerIndex_;    // 可写数据的起始位置
};