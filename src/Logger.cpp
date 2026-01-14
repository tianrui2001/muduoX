#include "Logger.h"

#include <iostream>

Logger& Logger::getInstance(){
    static Logger logger;
    return logger;
}

void Logger::setLogLevel(int level){
    logLevel_ = level;
}

void Logger::log(std::string msg){
    // 写日志： [日志级别] time ： msg
    switch (logLevel_)
    {
    case DEBUG:
        std::cout << "[DEBUG]";
        break;
    case INFO:
        std::cout << "[INFO]";
        break;
    case ERROR:
        std::cout << "[ERROR]";
        break;
    case FATAL:
        std::cout << "[FATAL]";
        break;
    default:
        break;
    }

    // 待实现 时间信息
    std::cout << " time :" << msg << std::endl;
}