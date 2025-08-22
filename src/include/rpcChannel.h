#pragma once

// 此类是继承自google::protobuf::RpcChannel
// 目的是为了给客户端进行方法调用的时候，统一接收的

#include <google/protobuf/service.h>
#include "zookeeperutil.h"

class rpcChannel : public google::protobuf::RpcChannel
{
public:
    rpcChannel(bool connectNow);
    virtual ~rpcChannel() {}
    // 所有通过stub代理对象调用的rpc方法，都走到这里，统一做rpc方法调用的数据序列化和网络发送
    void CallMethod(const ::google::protobuf::MethodDescriptor *method,
                    ::google::protobuf::RpcController *controller,
                    const ::google::protobuf::Message *request,
                    ::google::protobuf::Message *response,
                    ::google::protobuf::Closure *done) override;

private:
    int m_clientfd; // 存储客户端套接字
    std::string service_name;
    std::string m_ip;
    uint16_t m_port;
    std::string method_name;
    int m_idx; // 用于划分服务器IP和port的下标
    bool newConnect(const char *ip, uint16_t port);
    std::string QueryServiceHost(ZkClient *zkclient, std::string service_name,
                                 std::string method_name, int &idx);
};
