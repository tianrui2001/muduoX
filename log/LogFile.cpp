#include "LogFile.h"

#include <time.h>

LogFile::LogFile(const std::string& basename,
        off_t rollsize,
        int flushInterval,
        int checkEveryN)
    : basename_(basename),
      rollsize_(rollsize),
      flushInterval_(flushInterval),
      checkEveryN_(checkEveryN),
      count_(0),
      startOfPeriod_(0),
      lastRoll_(0),
      lastFlush_(0)
{
    // 重新启动时，可能没有log文件，因此在构建logFile对象，直接调用rollfile()创建一个新的log文件
    rollFile();
}


LogFile::~LogFile() = default;

void LogFile::append(const char* data, size_t len){
    std::lock_guard<std::mutex> lock(mutex_);
    appendInlock(data, len);
}

void LogFile::flush(){
    file_->flush();
}

// 滚动日志文件。 其实就是关闭旧的，创建新的日志文件
bool LogFile::rollFile(){
    time_t now = 0;
    std::string filename = getLogFileName(basename_, &now);
    time_t start = now / kRollPerSeconds_ * kRollPerSeconds_;

    if(now > lastRoll_){ // 防止频繁滚动日志文件
        lastRoll_ = now;
        lastFlush_ = now;
        startOfPeriod_ = start;
        file_.reset(new FileUtil(filename));
        return true;
    }

    return false;
}

std::string LogFile::getLogFileName(const std::string& basename, time_t* now){
    std::string filename;
    filename.reserve(basename_.size() + 64);
    filename = basename;

    char timebuf[32];
    struct tm tm;
    *now = time(NULL);
    localtime_r(now, &tm);  // 线程安全的localtime
    strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.log", &tm);

    filename += timebuf;
    filename += ".log";
    return filename;
}

void LogFile::appendInlock(const char* data, int len){
    file_->append(data, len);
    time_t now = ::time(NULL);
    count_++;   // 写入计数器加一

    if(file_->writtenBytes() > rollsize_){
        rollFile();     // 写入字数超过大小限制，滚动日志文件
    } else if(count_ >= checkEveryN_){
        count_ = 0;
        time_t thisPeriod = now / kRollPerSeconds_ * kRollPerSeconds_;  // 计算当前时间所属的周期起始时间
        if(thisPeriod != startOfPeriod_){
            rollFile(); // 跨天，滚动日志文件
        }
    }

    if(now - lastFlush_ > flushInterval_){
        lastFlush_ = now;
        flush();    // 超过刷新间隔，刷新日志文件
    }

}