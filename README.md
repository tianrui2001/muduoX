
---

# muduoX-HttpServer

**muduoX-HttpServer** 是一个基于 **C++11/14/17** 编写的高性能、全异步 Web 服务器。项目深度参考了经典网络库 `muduo` 的设计思想，并创新性地集成了 Linux 最新的 **`io_uring`** 技术，实现了网络 I/O 与磁盘 I/O 的全链路非阻塞处理。

---

## 🚀 项目亮点 (面试核心)

*   **全异步 I/O 架构**：不同于传统的 `epoll` + 阻塞式文件读取，本项目利用 `io_uring` 实现了真正的异步磁盘 I/O，彻底解决了 Reactor 线程在处理静态资源请求时的阻塞问题。
*   **高度解耦的设计**：基于 `One Loop Per Thread` 模型，将 TCP 连接管理、应用层 HTTP 协议解析（状态机实现）与业务逻辑完全分离。
*   **工业级日志系统**：实现了一个前端无阻塞、后端自动滚动的异步日志系统，支持多线程并发写入，峰值写入速度极高。
*   **自研内存池优化**：针对 `HttpContext` 等小对象的频繁申请与释放，实现了固定大小的哈希桶内存池，显著降低了堆内存碎片和分配开销。

---

## 🛠 核心技术栈

### 1. 网络层 (Transport Layer)
*   **Reactor 模式**：使用 `Epoll` (支持 LT/ET) 配合 `Non-blocking I/O` 的事件驱动架构。
*   **多线程模型**：采用 `Main-Reactor + Sub-Reactors` 模型，主线程负责 Accept，从线程（I/O 线程池）负责数据收发。
*   **Channel & Poller**：封装底层事件分发机制，通过回调函数解耦具体业务。

### 2. 异步 I/O 增强 (The io_uring Edge)
*   **桥接机制**：通过 `eventfd` 将 `io_uring` 的完成事件集成到 `Epoll` 事件循环中。
*   **全异步静态文件服务**：在处理静态资源请求（如 HTML、图片）时，通过 `io_uring` 发起异步读取，主线程立即返回继续处理其他连接，待内核读取完成后自动触发发送回调。

### 3. 应用层协议 (Application Layer)
*   **HTTP/1.1 协议解析**：实现了一个双层有限状态机（FSM）。主状态机处理 `RequestLine`、`Headers`、`Body`，微观状态机逐字节解析头部字段。
*   **长连接支持**：支持 `Keep-Alive`，通过 `TimerQueue` 管理非活跃连接的自动超时剔除。
*   **MimeType 识别**：内置线程安全的单例 `MimeType` 映射表，支持多种静态资源类型的自动识别。

### 4. 基础设施 (Infrastructure)
*   **AsyncLogging**：
    *   **双缓冲技术**：利用 `FixedBuffer` 在前端快速缓存日志，后端线程批量写入磁盘，减少系统调用。
    *   **日志滚动**：支持按文件大小或跨天自动滚动日志文件。
*   **MemoryPool**：
    *   基于 `std::vector` 的 64 级哈希桶内存池。
    *   针对 $O(1)$ 的小对象分配优化，结合 `shared_ptr` 的自定义删除器实现安全的资源回收。

---

## 🏗 架构示意图

```text
[Client] <--> [Acceptor] (Main Reactor)
                  |
        [EventLoopThreadPool] (Dispatching via Round-Robin)
                  |
        [EventLoop] (Sub Reactor) <---------- [eventfd] (Uring Notify)
           |          |                           ^
       [Channel] [TcpConnection]                  |
           |          |                    [io_uring Engine]
       [Socket]  [HttpContext] (FSM)              |
                      |                    [Disk Files (www)]
               [HttpServer::onRequest]
```

---

## 📈 性能调优记录

*   **解决异步内存竞态**：针对 `io_uring` 读操作完成时原始栈帧已销毁的问题，通过 `std::shared_ptr` 延长缓冲区生命周期。
*   **解决 HTTP 粘包/分包**：在 `onMessage` 中引入 `while` 循环解析，确保在一次读事件触发中处理完所有积压的完整报文。
*   **零拷贝思想**：在 `Buffer` 类的设计中大量应用 `std::swap` 和指针偏移，尽量减少应用层的数据拷贝。

---

## ⚡ 快速开始

### 依赖环境
*   Linux Kernel >= 5.1 (支持 io_uring)
*   `liburing-dev`
*   `cmake` >= 3.10
*   `g++` >= 7.5 (支持 C++14/17)

### 编译与运行
```bash
# 1. 编译核心库 muduoX
cd muduoX
sudo ./autobuild.sh

# 2. 编译并运行应用
cd http
make
./httpserver

# 3. 另开一个终端，目前支持下面这几个命令：
curl -v http://127.0.0.1:8000/index.html
curl -curl -v http://127.0.0.1:8000/api/hello
cucurl -v http://127.0.0.1:8000/api/version
```