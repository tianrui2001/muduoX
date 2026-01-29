#pragma once

#include <string>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <functional>

#include "LogSream.h"
#include "TimeStamp.h"

class SourceFile
{
public:
    SourceFile(const char* filename)
        :data_(filename)
    {
        const char* slash = strrchr(filename, '/'); // 查找最后一个斜杠的位置
        if(slash){
            data_ = slash + 1; // 如果找到了斜杠，更新data_指向斜杠后的文件名
        }

        size_ = static_cast<int>(strlen(data_));
    }

    const char* data_;
    int size_;
};

class Logger
{
public:
    enum LogLevel{
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        LEVEL_COUNT,
    };

    Logger(const char* filename, int line, LogLevel level);
    ~Logger();

    LogStream& stream() { return impl_.stream_; }

    // 输出函数和刷新缓冲区函数
    using OutputFunc = std::function<void(const char* msg, int len)>;
    using FlushFunc = std::function<void()>;
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);
private:
    class Impl{
        public:
        using LogLevel = Logger::LogLevel;

        Impl(LogLevel level, int savedErrno, const char* filename, int line);
        void formatTime();
        void finish();  //  添加一条log消息的后缀


        Timestamp time_;
        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

private:
    Impl impl_;
};

const char* getErrnoMsg(int savedErrno);

#define LOG_DEBUG Logger(__FILE__, __LINE__, Logger::DEBUG).stream()
#define LOG_INFO Logger(__FILE__, __LINE__, Logger::INFO).stream()
#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).stream()
#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).stream()
#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).stream()