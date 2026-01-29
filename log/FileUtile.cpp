#include "FileUtil.h"

#include <cstring>

FileUtil::FileUtil(std::string file_name)
    : writtenbytes_(0)
{
    file_ = ::fopen(file_name.c_str(), "ae"); // 'e'表示O_CLOEXEC
    if(file_){
        ::setbuffer(file_, buffer_, sizeof buffer_);    // 将我们自己的 buffer_ 设置为文件流的缓冲区
    }
}

FileUtil::~FileUtil(){
    if(file_){
        ::fclose(file_);
    }
}

void FileUtil::append(const char* data, size_t len){
    size_t writen = 0;
    while(writen < len){
        size_t remain = len - writen;
        size_t n = write(data + writen, remain);
        if(n != remain){
            int err = ferror(file_);

            // 写入失败
            if(err){
                fprintf(stderr, "FileUtil::append() failed %s\n", strerror(err));
                clearerr(file_);    // 清除文件指针的错误标志
                break;
            }

            // 不足写入
        }

        writen += n;
    }

    writtenbytes_ += writen;
}

void FileUtil::flush(){
    ::fflush(file_);    // 强制刷新缓冲区
}

size_t FileUtil::write(const char* data, size_t len){
    // 没用选择线程安全的fwrite()为性能考虑。
    // 在 data 缓冲区中将 len 个大小为 1 字节的数据项，写入到 file_ 所代表的文件流中去
    return ::fwrite_unlocked(data, 1, len, file_);
}