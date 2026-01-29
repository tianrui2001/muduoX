#include "LogSream.h"

#include <algorithm>

template<typename T>
void LogStream::formatInteger(T num){
    if(buffer_.avail() >= kMaxNumberSize){
        char* start = buffer_.current();
        char* cur = start;
        bool negative = num < 0;
        do{
            int digit = static_cast<int>(num % 10);
            num /= 10;
            (*cur++) = '0' + digit;
        }while(num != 0);

        if(negative) {
            (*cur++) = '-';
        }
        *cur = '\0';
        std::reverse(start, cur);
        buffer_.add(static_cast<size_t>(cur - start));
    }
}

LogStream& LogStream::operator<<(bool express){
    buffer_.append(express ? "true" : "false", express ? 4 : 5);
    return *this;
}

LogStream& LogStream::operator<<(short number){
    formatInteger(number);
    return *this;
}

LogStream& LogStream::operator<<(unsigned short number){
    formatInteger(number);
    return *this;   
}

LogStream& LogStream::operator<<(int number){
    formatInteger(number);
    return *this;
}

LogStream& LogStream::operator<<(unsigned int number){
    formatInteger(number);
    return *this;
}

LogStream& LogStream::operator<<(long number){
    formatInteger(number);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long number){
    formatInteger(number);
    return *this;
}

LogStream& LogStream::operator<<(long long number){
    formatInteger(number);
    return *this;
}

LogStream& LogStream::operator<<(unsigned long long number){
    formatInteger(number);
    return *this;
}

LogStream& LogStream::operator<<(float number){
    *this << static_cast<double>(number);
    return *this;
}

LogStream& LogStream::operator<<(double number){
    char buffer[32];
    sprintf(buffer, "%.12g", number);   // 最多输出12位有效数字, 去掉无意义的0
    buffer_.append(buffer, strlen(buffer));
    return *this;
}

LogStream& LogStream::operator<<(char str){
    buffer_.append(&str, 1);
    return *this;
}

LogStream& LogStream::operator<<(const char* str){
    buffer_.append(str, strlen(str));
    return *this;
}

LogStream& LogStream::operator<<(const unsigned char* str){
    buffer_.append(reinterpret_cast<const char*>(str), strlen(reinterpret_cast<const char*>(str)));
    return *this;
}

LogStream& LogStream::operator<<(const std::string& str){
    buffer_.append(str.c_str(), str.size());
    return *this;
}

LogStream& LogStream::operator<<(const GeneralTemplate& g){
    buffer_.append(g.data_, g.len_);
    return *this;
}
