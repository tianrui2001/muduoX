#include "HttpData.h"

#include <stdio.h>
#include <string.h>
#include <muduoX/Buffer.h>

pthread_once_t MimeType::once_control_ = PTHREAD_ONCE_INIT;
std::unordered_map<std::string, std::string> MimeType::mime_;

// 设置mime类型
void MimeType::init(){
    mime_[".html"] = "text/html";
    mime_[".avi"] = "video/x-msvideo";
    mime_[".bmp"] = "image/bmp";
    mime_[".c"] = "text/plain";
    mime_[".doc"] = "application/msword";
    mime_[".gif"] = "image/gif";
    mime_[".gz"] = "application/x-gzip";
    mime_[".htm"] = "text/html";
    mime_[".ico"] = "image/x-icon";
    mime_[".jpg"] = "image/jpeg";
    mime_[".png"] = "image/png";
    mime_[".txt"] = "text/plain";
    mime_[".mp3"] = "audio/mpeg";
    mime_["default"] = "text/html";
}

std::string MimeType::getMime(const std::string &suffix){
    pthread_once(&once_control_, MimeType::init);

    if(mime_.find(suffix) == mime_.end()){
        return mime_["default"];
    } else {
        return mime_[suffix];
    }
}
