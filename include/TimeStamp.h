#pragma once

#include <iostream>
#include <string>

class Timestamp {
    public:
    Timestamp() : microSecondsSinceEpoch_(0) {}
    explicit Timestamp(uint64_t microSecondsSinceEpoch) 
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
    static Timestamp now();
    std::string toString() const;
    bool valid() const { return microSecondsSinceEpoch_ > 0;}

    static const int kMicroSecondsPerSecond = 1000 * 1000;
    private:
    int64_t microSecondsSinceEpoch_;
};


inline Timestamp addTime(Timestamp timestamp, double seconds){
    int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

inline bool operator<(Timestamp lhs, Timestamp rhs){
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs){
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}