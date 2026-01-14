#include <muduoX/TcpServer.h>
#include <muduoX/Logger.h>
#include <string>
#include <functional>


class EchoService{
public:
    EchoService(EventLoop *loop,
                const InetAddress &listenAddr,
                const std::string &name)
        :loop_(loop),
        server_(loop, listenAddr, name)
    {
        server_.setConnectionCallback(
            std::bind(&EchoService::onConnection, this, std::placeholders::_1)
        );
        server_.setMessageCallback(
            std::bind(&EchoService::onMessage, this,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      std::placeholders::_3)
        );

        server_.setThreadNum(4); // 设置底层subLoop的数量
    }

    void start(){
        server_.start();
    }


private:
    // 连接回调函数
    void onConnection(const TcpConnectionPtr &conn){
        if(conn->connected()){
            LOG_INFO("EchoService - %s -> %s is online", 
                conn->peerAddr().toIpPort().c_str(), 
                conn->locallAddr().toIpPort().c_str());
        } else {
            LOG_INFO("EchoService - %s -> %s is offline", 
                conn->peerAddr().toIpPort().c_str(), 
                conn->locallAddr().toIpPort().c_str());
        }
    }

    // 消息回调函数
    void onMessage(const TcpConnectionPtr &conn, Buffer *buf, Timestamp timestamp){
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);
        conn->shutdown();   // 关闭连接，发送完数据后关闭连接
    }

    EventLoop *loop_;
    TcpServer server_;
};

int main(){
    EventLoop loop;
    InetAddress addr(8000);
    EchoService echoService(&loop, addr, "EchoService");
    echoService.start();
    loop.loop();
}