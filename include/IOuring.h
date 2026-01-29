#pragma once

#include <memory>
#include <linux/io_uring.h>
#include <liburing.h>
#include <string>
#include <vector>
#include <bits/types/struct_iovec.h>
#include <map>
#include <queue>

#include "Callbacks.h"
#include "Channel.h"


class IOContext;
class UringManager;
class EventLoop;

class RWOperation
{
public:
    enum Operation { READ, WRITE };

    RWOperation(Operation op, off_t off, const iovec &defaultIOv);
    void appendIOVec(const iovec &iov);

    Operation op() const { return op_; }
    off_t offset() const { return offset_; }
    const iovec* rawIov() const { return iovec_.data(); }
    size_t iovSize() const { return iovec_.size(); }

private:
    Operation op_;
    off_t offset_;
    std::vector<iovec> iovec_;
};


// 返回值和读写操作
using RWCallback = std::function<void(int, RWOperation&)>;

class File : public std::enable_shared_from_this<File>
{
public:
    File(int fd, UringManager* uringManager)
    : fd_(fd), uringManager_(uringManager) {}

    ~File() { ::close(fd_); }

    void asynRW(RWOperation&& op, RWCallback &&cb);

    int fd() const { return fd_; }

private:
    int fd_;
    UringManager* uringManager_;
};


class IOContext 
{
public:
    IOContext(std::shared_ptr<File> file, const RWOperation &&rwOp, RWCallback &&cb);
    void runCallback(int retval);

    File* file() const {return file_.get(); }
    const RWOperation& rwOp() const { return rwOp_; }

private:
    std::shared_ptr<File> file_;
    RWOperation rwOp_;
    RWCallback callback_;
};


// IOUring管理类，负责提交和处理IO请求
// 不是线程安全的，只能在这个EventLoop中运行
class UringManager
{
public:
    explicit UringManager(EventLoop* loop);
    ~UringManager();

    std::shared_ptr<File> registerFile(const std::string& filePath);

private:
    void appendIOContext(IOContext &&ctx);
    void handEventRead();
    void handleCQE();

    friend class File;

    struct io_uring ring_;
    int eventfd_;
    Channel channel_;
    EventLoop *loop_;


    // key：唯一的请求ID，用作 io_uring 的 user_data
    std::map<int, IOContext> activeIOs_; 
    size_t currentIdx_; // 自增计数器, 作为每个IO请求的唯一ID

    std::queue<IOContext> pendingIOs_; // 当SQE放不下时，暂存的IO请求
    static const size_t URING_ENTRYS = 8;
};