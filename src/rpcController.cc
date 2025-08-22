#include "rpcController.h"

// 构造函数，初始化控制器状态
rpcController::rpcController()
{
    m_failed = false;
    m_errText = "";
}

// 重置控制器状态，将失败标志和错误信息清空
void rpcController::Reset()
{
    m_failed = false;
    m_errText = "";
}

// 判断当前RPC调用是否失败
bool rpcController::Failed() const
{
    return m_failed;  // 返回失败标志
}

// 获取错误信息
std::string rpcController::ErrorText() const
{
    return m_errText; 
}

// 设置RPC调用失败，并记录失败原因
void rpcController::SetFailed(const std::string &reason)
{
    m_failed = true;
    m_errText = reason;
}

// 以下功能未实现，是RPC服务端提供的取消功能
// 开始取消RPC调用（未实现）
void rpcController::StartCancel() {
    // 目前为空，未实现具体功能
}

// 判断RPC调用是否被取消（未实现）
bool rpcController::IsCanceled() const {
    return false;  // 默认返回false，表示未被取消
}

// 注册取消回调函数（未实现）
void rpcController::NotifyOnCancel(google::protobuf::Closure* callback) {
    // 目前为空，未实现具体功能
}