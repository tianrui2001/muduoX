#include "MemoryPool.h"

MemoryPool::MemoryPool(size_t BlockSize)
    : BlockSize_(BlockSize) {}

MemoryPool::~MemoryPool(){
    // 把连续的block删除
    Slot* cur = firstBlock_;
    while(cur){
        Slot* next = cur->next;

        // 转化为 void 指针，因为 void 类型不需要调用析构函数，只释放空间
        ::operator delete(reinterpret_cast<void*>(cur));
        cur = next;
    }
}

void MemoryPool::init(size_t slotSize){
    SlotSize_ = slotSize;
    if(SlotSize_ < sizeof(Slot*)){
        SlotSize_ = sizeof(Slot*);
    }
}

void* MemoryPool::allocate(){
    // 优先使用空闲链表中的内存槽
    if(freeList_){
        {
            std::lock_guard<std::mutex> lock(mutexForFreeList_);
            if(freeList_){
                Slot* temp = freeList_;
                freeList_ = freeList_->next;
                return temp;
            }
        }
    }

    Slot* temp;
    {
        std::lock_guard<std::mutex> lock(mutexForBlock_);
        if(curSlot_ >= lastSlot_){  // 需要分配新块
            allocateNewBlock();
        }

        temp = reinterpret_cast<Slot*>(curSlot_);
        curSlot_ += SlotSize_;
    }
    return temp;
}

void MemoryPool::deallocate(void* ptr){
    if(ptr){
        std::lock_guard<std::mutex> lock(mutexForFreeList_);
        reinterpret_cast<Slot*>(ptr)->next = freeList_;
        freeList_ = reinterpret_cast<Slot*>(ptr);
    }
}

void MemoryPool::allocateNewBlock(){
    Slot* newBlock = reinterpret_cast<Slot*>(::operator new(BlockSize_));
    newBlock->next = firstBlock_;
    firstBlock_ = newBlock;

    char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);

    // 让指针对齐到 SlotSize_ 的整数倍位置
    size_t panddingSize = (SlotSize_ - reinterpret_cast<size_t>(body) % SlotSize_) % SlotSize_;
    curSlot_ = body + panddingSize;
    lastSlot_ = reinterpret_cast<char*>(newBlock) + BlockSize_ - SlotSize_ + 1;

    freeList_ = nullptr;
}

void HashBucket::initMemoryPools(){
    for(int i=0; i < MEMORY_POOL_NUM; i++){
        getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
    }
}

MemoryPool& HashBucket::getMemoryPool(int index){
    static MemoryPool memoryPools[MEMORY_POOL_NUM];
    return memoryPools[index];
}