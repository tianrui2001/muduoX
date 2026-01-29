#include "Logger.h"
#include "CurrentThread.h"

thread_local char t_errnobuf[512];  // 每个线程独立的错误信息缓冲
thread_local char t_time[32];   // 每个线程独立的时间格式化缓冲区
thread_local time_t t_lastSecond;   // 每个线程记录上次格式化的时间


const char* getErrnoMsg(int savedErrno){
    return strerror_r(savedErrno, t_errnobuf, sizeof(t_errnobuf));
}

// 根据Level 返回level_名字
const char* LogLevelName[Logger::LEVEL_COUNT] = {
    " [TRACE] ",
    " [DEBUG] ",
    " [INFO ] ",
    " [WARN ] ",
    " [ERROR] ",
    " [FATAL] ",
};

/**
 * 默认的日志输出函数
 * 将日志内容写入标准输出流(stdout)
 * @param data 要输出的日志数据
 * @param len 日志数据的长度W
 */
static void defaultOutput(const char* msg, int len){
    fwrite(msg, 1, len, stdout);
}

/**
 * 默认的刷新函数
 * 刷新标准输出流的缓冲区,确保日志及时输出
 * 在发生错误或需要立即看到日志时会被调用
 */
static void defaultFlush(){
    fflush(stdout);
}

Logger::OutputFunc g_output = defaultOutput;
Logger::FlushFunc g_flush = defaultFlush;

Logger::Impl::Impl(LogLevel level, int savedErrno, const char* filename, int line)
    : time_(Timestamp::now()),
        level_(level),
        line_(line),
        basename_(filename)
{
    formatTime();
    stream_ << LogLevelName[level];

    if(savedErrno != 0){
        stream_ << getErrnoMsg(savedErrno) << " (errno=" << savedErrno << ") ";
    }
}

void Logger::Impl::formatTime(){
    // 获取当前时间戳的秒数和微妙数
    Timestamp now = Timestamp::now();
    time_t seconds = static_cast<time_t>(now.microSecondsSinceEpoch() / Timestamp::kMicroSecondsPerSecond);
    int microseconds = static_cast<int>(now.microSecondsSinceEpoch() % Timestamp::kMicroSecondsPerSecond);

    std::string timeStr = now.toString();
    strncpy(t_time, timeStr.c_str(), sizeof(t_time) - 1);
    t_time[sizeof(t_time) - 1] = '\0'; // 确保一定有结束符

    // 更新最后一次时间调用
    t_lastSecond = seconds;

    char buf[32] = {0};
    sprintf(buf, ".%06d ", microseconds);

    stream_ << GeneralTemplate(t_time, 19) << GeneralTemplate(buf, 7);
}

void Logger::Impl::finish(){
    stream_ << " - " <<  GeneralTemplate(basename_.data_, basename_.size_) << ":" << line_ << "\n";
}

Logger::Logger(const char* filename, int line, LogLevel level)
    : impl_(level, 0, filename, line) {}

Logger::~Logger(){
    impl_.finish();
    const LogStream::Buffer& buffer = stream().buffer();
    g_output(buffer.data(), buffer.length());
    if(impl_.level_ == FATAL){
        g_flush();
        abort();
    }
}

void Logger::setOutput(OutputFunc out){
    g_output = out;
}

void Logger::setFlush(FlushFunc flush){
    g_flush = flush;
}