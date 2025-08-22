// 为服务器通过服务的函数

#pragma once
#include "rpcConfig.h"
#include "rpcChannel.h"
#include "rpcController.h"
#include <mutex>
// Krpc基础类，负责框架的一些初始化操作
class rpcApplication
{
public:
    static void Init(int argc, char **argv);
    static rpcApplication &GetInstance();
    static void deleteInstance();
    static rpcConfig &GetConfig();
private:
    static rpcConfig m_config;
    static rpcApplication *m_application;//全局唯一单列访问对象
    static std::mutex m_mutex;
    rpcApplication(){}
    ~rpcApplication(){}
    rpcApplication(const rpcApplication&)=delete;
    rpcApplication(rpcApplication&&)=delete;
};