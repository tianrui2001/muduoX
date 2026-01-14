#include "TimeStamp.h"

#include <string>
#include <sys/time.h>

Timestamp Timestamp::now(){
    // 获取当前时间戳
    return Timestamp(time(NULL));   
}


std::string Timestamp::tostring() const {
    char buf[64] = {0};
    tm *tm_time = localtime(&microSecondsSinceEpoch_);
    snprintf(buf, sizeof(buf), "%4d-%02d-%02d %02d:%02d:%02d",
             tm_time->tm_year + 1900,
             tm_time->tm_mon + 1,
             tm_time->tm_mday,
             tm_time->tm_hour,
             tm_time->tm_min,
             tm_time->tm_sec);
    return buf;
}