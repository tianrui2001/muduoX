#pragma once

#include <string>
#include <string.h>

#include "Nocopyable.h"
#include "FixedBuffer.h"

class GeneralTemplate:nocopyable
{
public:
    GeneralTemplate()
        : data_(nullptr),
          len_(0) {}
          
    GeneralTemplate(const char* data, int len)
        : data_(data),
          len_(len) {}

    const char* data_;
    int len_;
};

// LogStream类用于管理日志输出流，重载输出流运算符<<，将各种类型的值写入FixedBuffer缓冲区
class LogStream : nocopyable
{
public:
    using Buffer = FixedBuffer<kSmallBuffer>;

    void append(const char* data, int len){
        buffer_.append(data, len);
    }

    const Buffer& buffer() const { return buffer_; }
    void resetBuffer() { buffer_.reset(); }

    // 重载输出流运算符<<，支持多种数据类型的写入
    LogStream& operator<<(bool express);

    LogStream& operator<<(short number);
    LogStream& operator<<(unsigned short number);
    LogStream& operator<<(int number);
    LogStream& operator<<(unsigned int number);
    LogStream& operator<<(long number);
    LogStream& operator<<(unsigned long number);
    LogStream& operator<<(long long number);
    LogStream& operator<<(unsigned long long number);

    LogStream& operator<<(float number);
    LogStream& operator<<(double number);

    LogStream& operator<<(char str);
    LogStream& operator<<(const char* str);
    LogStream& operator<<(const unsigned char* str);
    LogStream& operator<<(const std::string& str);
    LogStream& operator<<(const GeneralTemplate& g);

private:

    static constexpr int kMaxNumberSize = 32; // 数字转换为字符串的最大长度

    template<typename T>
    void formatInteger(T num);  // 整数转字符串

    Buffer buffer_;  // 用于存储日志内容的缓冲区
};