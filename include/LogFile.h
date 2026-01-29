#pragma once

#include <mutex>
#include <memory>
#include <ctime>

#include "FileUtil.h"


/**
 * @brief 日志文件管理类
 * 负责日志文件的创建、写入、滚动和刷新等操作
 * 支持按大小和时间自动滚动日志文件
 */
class LogFile
{
public:
    LogFile(const std::string& basename,
            off_t rollsize,
            int flushInterval = 3,
            int checkEveryN = 1024);
    ~LogFile();

    void append(const char* data, size_t len);

    // 强制将缓冲区数据刷新到磁盘
    void flush();

    /**
     * @brief 滚动日志文件
     * 当日志文件大小超过rollsize_或时间超过一天时，创建新的日志文件
     * @return 是否成功滚动日志文件
     */
    bool rollFile();


private:

    /**
     * @brief 生成日志文件名
     * @param basename 日志文件基本名称
     * @param now 当前时间指针
     * @return 完整的日志文件名，格式为:basename.YYYYmmdd-HHMMSS.log
     */
    std::string getLogFileName(const std::string& basename, time_t* now);

    /**
     * @brief 在已加锁的情况下追加数据
     * @param data 要写入的数据
     * @param len 数据长度
     */
    void appendInlock(const char* data, int len);

    const std::string basename_;
    const off_t rollsize_;  // 滚动文件的大小
    const int flushInterval_;   // 刷新间隔，默认为3秒
    const int checkEveryN_;   // 每N次写入检查一次是否需要滚动或刷新, 默认为1024

    int count_; // 写入计数器, 超过限值checkEveryN_时清除, 然后重新计数

    std::mutex mutex_;
    time_t startOfPeriod_; // 本次写log周期的起始时间(秒)
    time_t lastRoll_;  // 上次滚动时间(秒)
    time_t lastFlush_; // 上次刷新时间(秒)
    std::unique_ptr<FileUtil> file_; // 文件操作类
    const static int kRollPerSeconds_ = 60 * 60 * 24; // 日志滚动周期，默认为一天
};