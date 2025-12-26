#pragma once

// 抽象类，禁止拷贝构造和拷贝赋值
class nocopyable {
public:
    nocopyable(const nocopyable&) = delete;
    nocopyable& operator=(const nocopyable&) = delete;
protected:
    nocopyable() = default;
    ~nocopyable() = default;

};