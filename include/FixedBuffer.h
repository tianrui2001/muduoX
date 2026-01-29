#pragma once 

#include <string.h>
#include <string>

#include "Nocopyable.h"

class AsyncLogging;
const int kSmallBuffer = 4000; // 4KB
const int kLargeBuffer = 4000 * 1000; // 4MB

template<int bufferz_size>
class FixedBuffer : nocopyable
{
public:
    FixedBuffer()
        : cur_(data_)
    {}

    void append(const char* buf, size_t len){
        if(avail() > static_cast<int>(len)){
            ::memcpy(cur_, buf, len);
            add(len);
        }
    }

    // 返回缓冲区的起始地址
    const char* data() const { return data_; }

    // 返回缓冲区中当前有效数据的长度
    int length() const { return static_cast<int>(cur_ - data_); }

    // 返回当前指针的位置
    char* current() { return cur_; }

    // 返回缓冲区中剩余可用空间的大小
    int avail() const { return static_cast<int>(bufferz_size - length()); }

    // 更新当前指针，增加指定长度
    void add(size_t len) { cur_ += len; }

    // 重置当前指针，回到缓冲区的起始位置
    void reset() { cur_ = data_; }

    // 清空缓冲区的数据
    void bzero() { ::memset(data_, 0, sizeof data_); }
    
    // 将缓冲区中的数据转换为std::string类型并返回
    std::string toString() const { return std::string(data_, length()); }
private:

    /**
     * ---------------------------------------------------------
     * |** data_ **|                                           |
     * ---------------------------------------------------------
     *             ^                                           ^
     *             |>>>>>>>>>>>>>>>>>>  avail  <<<<<<<<<<<<<<<<|
     *            cur_                            
     */
    char data_[bufferz_size];   // 定义固定大小的缓冲区
    char* cur_;  // 指向当前缓冲区位置
};