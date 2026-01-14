#pragma once

#include <iostream>
#include <string>

class Timestamp {
    public:
    Timestamp() : microSecondsSinceEpoch_(0) {}
    explicit Timestamp(uint64_t microSecondsSinceEpoch) 
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

    static Timestamp now();
    std::string tostring() const;

    private:
    int64_t microSecondsSinceEpoch_;
};