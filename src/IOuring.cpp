#include "IOuring.h"
#include "Logger.h"
#include "EventLoop.h"

#include <unistd.h>
#include <sys/eventfd.h>
#include <liburing.h>
#include <fcntl.h> // for open
#include <sys/eventfd.h>

static int createEventfd(){
  int eventfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if(eventfd < 0){
    LOG_FATAL << "Failed in eventfd\n";
  }

  return eventfd;
}

RWOperation::RWOperation(Operation op, off_t off, const iovec &defaultIOv)
    : op_(op),
      offset_(off)
{
	iovec_.emplace_back(defaultIOv);
}

void RWOperation::appendIOVec(const iovec &iov)
{
	iovec_.emplace_back(iov);
}

void File::asynRW(RWOperation&& op, RWCallback &&cb){
  IOContext ioCtx(shared_from_this(), std::move(op), std::move(cb));
  uringManager_->appendIOContext(std::move(ioCtx)); // 提交给UringManager处理
}


IOContext::IOContext(std::shared_ptr<File> file, const RWOperation &&rwOp, RWCallback &&cb)
  : file_(file),
    rwOp_(std::move(rwOp)),
    callback_(std::move(cb))
{}

void IOContext::runCallback(int retval){
  callback_(retval, rwOp_);
}


UringManager::UringManager(EventLoop* loop)
  : eventfd_(createEventfd()),
    channel_(loop, eventfd_),
    loop_(loop),
    currentIdx_(0)
{
  // 初始化io_uring
  if(io_uring_queue_init(URING_ENTRYS, &ring_, 0) < 0){
    LOG_FATAL << "io_uring_queue_init failed";
  }

  // 注册eventfd到io_uring, 当CQ从空变成非空时会通知eventfd
  if(io_uring_register_eventfd(&ring_, eventfd_) < 0){
    LOG_FATAL << "io_uring_register_eventfd failed";
  }

  channel_.setReadCallback(
    std::bind(&UringManager::handEventRead, this)
  );
  channel_.enableReading();
}

UringManager::~UringManager(){
  channel_.disableAll();
  channel_.remove();

  io_uring_unregister_eventfd(&ring_);
  io_uring_queue_exit(&ring_);

  ::close(eventfd_);
}

std::shared_ptr<File> UringManager::registerFile(const std::string& filePath){
  int fd = ::open(filePath.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
  if(fd < 0){
    LOG_ERROR << "Failed to open file: " << filePath;
    return std::shared_ptr<File>();
  }

  return std::make_shared<File>(fd, this);
}

void UringManager::appendIOContext(IOContext &&ctx){
  loop_->runInLoop([this, ctx{std::move(ctx)}](){
    io_uring_sqe* sqe = io_uring_get_sqe(&ring_); // 获取可用的SQE
    if(sqe == nullptr){
      pendingIOs_.push(std::move(ctx));
    } else {
      const RWOperation& rwOp = ctx.rwOp();
      int fd = ctx.file()->fd();

      if(rwOp.op() == RWOperation::READ){
        io_uring_prep_readv(sqe, fd, rwOp.rawIov(), rwOp.iovSize(), rwOp.offset());
      } else if(rwOp.op() == RWOperation::WRITE){
        io_uring_prep_writev(sqe, fd, rwOp.rawIov(), rwOp.iovSize(), rwOp.offset());
      }

      size_t ioIdx = currentIdx_++;
      io_uring_sqe_set_data(sqe, reinterpret_cast<void*>(static_cast<uintptr_t>(ioIdx))); // 设置用户数据
      int ret = io_uring_submit(&ring_);
      if(ret < 0){
        LOG_ERROR << "io_uring_submit failed: " << ret;
      }

      activeIOs_.emplace(ioIdx, std::move(ctx));// 保存IOContext
    }
  });
}

void UringManager::handEventRead(){
  uint64_t buf;
  ssize_t n = ::read(eventfd_, &buf, sizeof(buf));
  if(n != sizeof(buf)){
    LOG_FATAL << "UringManager::handEventRead read " << n << " bytes instead of 8";
  }

  handleCQE();
}

void UringManager::handleCQE(){
  io_uring_cqe* cqe = nullptr;
  while(true){
    int ret = io_uring_peek_cqe(&ring_, &cqe);  // 瞄一眼有没有完成的请求
    if(ret == -EAGAIN){
      break; // 没有更多完成的请求
    } else if(ret < 0){
      LOG_ERROR << "io_uring_peek_cqe failed: " << ret;
      break;
    } else {
      // 获取全局唯一的请求号，在 activeIOs_ map中找到对应的请求上下文，从而知道这个请求的信息
      size_t ioIdx = static_cast<size_t>(reinterpret_cast<uintptr_t>(io_uring_cqe_get_data(cqe)));
      IOContext& ioCtx = activeIOs_.at(ioIdx);
      ioCtx.runCallback(cqe->res); // 执行回调函数
      activeIOs_.erase(ioIdx); // 移除已完成的IO请求
      io_uring_cqe_seen(&ring_, cqe); // 标记为已处理

      if(!pendingIOs_.empty()){
        IOContext nextCtx = std::move(pendingIOs_.front());
        pendingIOs_.pop();
        appendIOContext(std::move(nextCtx));
      }
    }
  }
}