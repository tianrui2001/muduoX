#pragma once

#include <string>
#include <stdio.h>
#include <sys/types.h> // off_t

/**
 * @brief 文件工具类，用于处理文件的写入操作
 * 该类封装了对文件的基本操作，包括写入数据和刷新缓冲区
 */
class FileUtil
{
public:
    /**
     * @brief 构造函数
     * @param file_name 要打开的文件名
     */
    FileUtil(std::string file_name);

    /**
     * @brief 析构函数
     * 负责关闭文件和清理资源
     */
    ~FileUtil();

     /**
     * @brief 向文件写入数据
     * @param data 要写入的数据的指针
     * @param len 要写入的数据长度
     */
    void append(const char* data, size_t len);

    void flush();

    off_t writtenBytes() const { return writtenbytes_; }

private:
    size_t write(const char* data, size_t len);

    FILE* file_;    // 文件指针，用于操作文件
    char buffer_[64 * 1024];   //  文件操作的缓冲区，用于提高写入效率
    off_t writtenbytes_;    // 已写入的字节数
};