#pragma once 

#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>

#define MEMORY_POOL_NUM 64
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

/**
 * 当内存空闲的时候就把它解释成 Slot 链表串起来，
 * 用的时候就从链表头取下一个分配给用户，并将它解释成用户需要的类型。
 * 也用于分配 Block 的时候，把 Block 串成链表
 */
struct Slot
{
    Slot* next;
};

class MemoryPool
{
public:
    MemoryPool(size_t BlockSize = 4096);
    ~MemoryPool();

    void init(size_t slotSize);
    void* allocate();
    void deallocate(void* ptr);

private:
    void allocateNewBlock();

    int     BlockSize_; // 内存块大小
    int     SlotSize_;  // 每个槽的大小
    Slot*   firstBlock_ = nullptr; // 指向第一个内存块
    Slot*   freeList_ = nullptr; // 空闲槽(使用后又被释放的槽)
    char*   curSlot_ = nullptr;  // 指向当前可用的槽
    char*   lastSlot_ = nullptr; // 作为当前内存块中最后能够存放元素的位置标识(超过该位置需申请新的内存块)
    std::mutex mutexForFreeList_;   // 保证freeList_在多线程中操作的原子性
    std::mutex mutexForBlock_;      // 保证多线程情况下避免不必要的重复开辟内存导致的浪费行为
};

class HashBucket
{
public:
    static void initMemoryPools();
    static MemoryPool& getMemoryPool(int index);

    static void* useMemory(size_t size){
        if(size < 0){
            return nullptr;
        }

        if(size > MAX_SLOT_SIZE){
            return ::operator new(size);
        }

        // 根据 size 到对应的桶里面去取内存
        return getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).allocate();
    }

    static void freeMemory(void* ptr, size_t size){
        if(!ptr){
            return;
        }

        if(size > MAX_SLOT_SIZE){
            ::operator delete(ptr);
            return;
        }

        getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
    }

    template<typename T, typename... Args>
    friend T* newElement(Args&&... args);

    template<typename T>
    friend void deleteElement(T* ptr);
};


template<typename T, typename... Args>
T* newElement(Args&&... args){
    T* p = nullptr;

    // 根据元素大小选取合适的内存池分配内存
    if((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr){
        // 在分配的内存上构造对象
        new (p) T(std::forward<Args>(args)...);
    }

    return p;
}

template<typename T>
void deleteElement(T* ptr){
    if(ptr){
        // 调用析构函数
        ptr->~T();
        // 释放内存
        HashBucket::freeMemory(reinterpret_cast<void*>(ptr), sizeof(T));
    }
}