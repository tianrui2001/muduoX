#pragma once 

template<typename Key, typename Value>
class CachePolicy{
public:
    virtual ~CachePolicy() = default;

    virtual void put(Key key, Value vlaue) = 0;
    virtual bool get(Key key, Value& value) = 0;
    virtual Value ger(Key key) = 0;
};