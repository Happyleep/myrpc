#pragma once

#include <glog/logging.h>
#include <string>

// 采用RAII的思想
class rpcLogger
{
public:
    explicit rpcLogger(const char *argv0)
    {
        google::InitGoogleLogging(argv0);
        FLAGS_colorlogtostderr = true; // 启用彩色日志
        FLAGS_logtostderr = true;      // 默认输出标准错误
    }
    ~rpcLogger()
    {
        google::ShutdownGoogleLogging();
    }

    // 提供静态日志方法
    static void Info(const std::string &message)
    {
        LOG(INFO) << message;
    }
    static void Warning(const std::string &message)
    {
        LOG(WARNING) << message;
    }
    static void ERROR(const std::string &message)
    {
        LOG(ERROR) << message;
    }
    static void Fatal(const std::string &message)
    {
        LOG(FATAL) << message;
    }

    // 禁用拷贝构造函数和重载赋值函数
private:
    rpcLogger(const rpcLogger &) = delete;
    rpcLogger &operator=(const rpcLogger &) = delete;
};