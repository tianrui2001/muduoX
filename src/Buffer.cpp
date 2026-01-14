#include "Buffer.h"

#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>


/**
 * 从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区是有大小的！ 但是从fd上读取数据的时候 却不知道tcp数据的最终大小
 *
 * @description: 从socket读到缓冲区的方法是使用readv先读至buffer_，
 * Buffer_空间如果不够会读入到栈上65536个字节大小的空间，然后以append的
 * 方式追加入buffer_。既考虑了避免系统调用带来开销，又不影响数据的接收。
 **/
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    // 栈额外空间，用于从套接字往出读时，当buffer_暂时不够用时暂存数据，
    //待buffer_重新分配足够空间后，在把数据交换给buffer_。
    char extrabuf[66536]; // 64KB栈空间

    /*
    struct iovec {
        ptr_t iov_base; // 指向缓冲区存放的readv所接收的数据或是writev将要发送的数据
        size_t iov_len; // 接收的最大长度以及实际写入的长度
    };
    */

    // 使用iovec分配两个连续的缓冲区
    struct iovec vec[2];
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    // 怕第一块空间不够用，第二块空间放在栈上
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf) ? 2 : 1);
    const ssize_t n = ::readv(fd, vec, iovcnt);
    if( n < 0){
        *saveErrno = errno;
    }else if (n <= static_cast<ssize_t>(writable)){
        // Buffer的可写缓冲区已经够存储读出来的数据了
        writerIndex_ += n;
    } else {
        // extrabuf里面也写入了n-writable长度的数据
        // 对buffer_扩容 并将extrabuf存储的另一部分数据追加至buffer_
        writerIndex_ += writable;
        append(extrabuf, n - writable);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0){
        *saveErrno = errno;
    }

    return n;
}