#pragma once

#include <map>
#include <string>
#include <unordered_map>
#include <pthread.h>
#include <algorithm>
#include <muduoX/TimeStamp.h>

class MimeType
{
public:
    static std::string getMime(const std::string &suffix);

private:
    MimeType() = default;
    ~MimeType() = default;
    static void init();

private:
    static std::unordered_map<std::string, std::string> mime_;
    static pthread_once_t once_control_;    // 保证只初始化一次
};

