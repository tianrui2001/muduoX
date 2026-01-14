#pragma once

#include <string>

#include "Nocopyable.h"


#define LOG_INFO(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(INFO); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while (0);

#ifdef MUDEBUG
#define LOG_DEBUG(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(DEBUG); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while (0);
#else
    #define LOG_DEBUG(logmsgFormat, ...)
#endif

#define LOG_ERROR(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(ERROR); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
    } while (0);

#define LOG_FATAL(logmsgFormat, ...) \
    do \
    { \
        Logger &logger = Logger::getInstance(); \
        logger.setLogLevel(FATAL); \
        char buf[1024] = {0}; \
        snprintf(buf, sizeof(buf), logmsgFormat, ##__VA_ARGS__); \
        logger.log(buf); \
        exit(-1); \
    } while (0);    

enum LogLevel {
    DEBUG,
    INFO,
    ERROR,
    FATAL
};

class Logger : nocopyable 
{
public:
    static Logger& getInstance();
    void setLogLevel(int level);
    void log(std::string msg);



private:
    int logLevel_;
    Logger(){};

};