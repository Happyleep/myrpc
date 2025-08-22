#pragma once

#include "google/protobuf/service.h"
#include "zookeeperutil.h"
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <google/protobuf/descriptor.h>
#include <functional>
#include <string>
#include <unordered_map>

// 在protobuf中使用google::protobuf::MethodDescriptor类来描述一个方法，
// 在protobuf中用google::protobuf::Service类来描述一个服务对象。

// 向外暴露 RPC 服务、接收客户端请求、派发到对应服务方法，并返回结果
class rpcProvider
{
public:
    // 这里是提供给外部使用的，可以发布rpc方法的函数接口。
    void NotifyService(google::protobuf::Service *service);
    ~rpcProvider();
    // 启动rpc服务节点，开始提供rpc远程网络调用服务
    // 开启提供rpc远程网络调用服务（利用muduo库提供的网络模块来监听网络事件，
    // 即监听来自远端的服务方法调用）
    void run();

private: 
    // 
    muduo::net::EventLoop eventLoop;
    // 用来临时存放本次注册的服务信息
    struct ServiceInfo
    {   // service服务类型信息
        google::protobuf::Service *service;// 保存服务对象
        std::unordered_map<std::string
        ,const google::protobuf::MethodDescriptor *> 
        methodMap;// 保存服务方法描述
    };

    // callee接收来自caller的请求服务方法，
    // callee根据服务方法名从m_serviceMap中定位到caller请求的服务对象及方法对象，然后进行调用。
    // 存储注册成功的服务对象和其服务方法的所有信息
    std::unordered_map<std::string, ServiceInfo> serviceMap;

    // 新的socket连接回调
    void OnConnection(const muduo::net::TcpConnectionPtr &conn);

    // 已建立连接用户的读写事件回调
    void OnMessage(const muduo::net::TcpConnectionPtr &conn
        ,muduo::net::Buffer *buffer
        ,muduo::Timestamp receiveTime);

    // Closure的回调操作，用于序列化rpc的响应和网络发送
    void SendRpcResponse(const muduo::net::TcpConnectionPtr &connect
        ,google::protobuf::Message *response);
};