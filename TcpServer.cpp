#include "TcpServer.h"


TcpServer::TcpServer(EventLoop *loop,
            const InetAddress &listenAddr,
            const std::string &nameArg,
            Option option){}

TcpServer::~TcpServer(){

}

void TcpServer::setThreadNum(int numThreads){

}

void TcpServer::start(){

}

void TcpServer::newConnection(int sockfd, const InetAddress &peerAddr){

}

void TcpServer::removeConnection(const TcpConnectionPtr &conn){

}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr &conn){
    
}