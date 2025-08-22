#include "rpcApplication.h"
#include "../user.pb.h"
#include "rpcController.h"
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include "rpcLogger.h"

// 发送 RPC 请求的函数，模拟客户端调用远程服务
void send_request(int thread_id, std::atomic<int> &success_count, std::atomic<int> &fail_count)
{
    // 创建一个 UserServiceRpc_Stub 对象，用于调用远程的 RPC 方法
    user::UserServiceRpc_Stub stub(new rpcChannel(false));

    // 设置 RPC 方法的请求参数
    user::LoginRequest request;
    request.set_name("happyhao");
    request.set_pwd("12345");
    
    // 定义rpc方法的响应参数
    user::LoginResponse response;
    rpcController controller; // 创建控制器对象， 用于处理rpc调用过程中的错误

    // 调用远程的Login方法
    // 发起rpc方法的调用，同步rpc调用过程，rpcChannel::callmethod
    stub.Login(&controller, &request, &response, nullptr);

    // 检查rpc调用是否成功
    if (controller.Failed())
    {
        std::cout << controller.ErrorText() << std::endl; // 打印错误信息
        fail_count++;                                     // 失败计数加 1
    }

    else
    { // 如果调用成功
        if (0 == response.result().errcode())
        {                                                                                  // 检查响应中的错误码
            std::cout << "rpc login response success:" << response.success() << std::endl; // 打印成功信息
            success_count++;                                                               // 成功计数加 1
        }
        else
        {                                                                                          // 如果响应中有错误
            std::cout << "rpc login response error : " << response.result().errmsg() << std::endl; // 打印错误信息
            fail_count++;                                                                          // 失败计数加 1
        }
    }
}

int main(int argc, char **argv)
{
    // 初始化 RPC 框架，解析命令行参数并加载配置文件
    // 整个程序启动以后，想使用rpc框架享受rpc服务调用，一定要先调用框架的初始化函数
    rpcApplication::Init(argc, argv);

    // 创建日志对象
    rpcLogger logger("MyRPC");

    const int thread_count = 10;       // 并发线程数
    const int requests_per_thread = 1; // 每个线程发送的请求数

    std::vector<std::thread> threads;  // 存储线程对象的容器
    std::atomic<int> success_count(0); // 成功请求的计数器
    std::atomic<int> fail_count(0);    // 失败请求的计数器

    auto start_time = std::chrono::high_resolution_clock::now(); // 记录测试开始时间

    // 启动多线程进行并发测试
    for (int i = 0; i < thread_count; i++)
    {
        threads.emplace_back([argc, argv, i, &success_count, &fail_count, requests_per_thread]()
                             {
            for (int j = 0; j < requests_per_thread; j++) {
                send_request(i, success_count, fail_count);  // 每个线程发送指定数量的请求
            } });
    }

    // 等待所有线程执行完毕
    for (auto &t : threads)
    {
        t.join();
    }

    auto end_time = std::chrono::high_resolution_clock::now();     // 记录测试结束时间
    std::chrono::duration<double> elapsed = end_time - start_time; // 计算测试耗时

    // 输出统计结果
    LOG(INFO) << "Total requests: " << thread_count * requests_per_thread;          // 总请求数
    LOG(INFO) << "Success count: " << success_count;                                // 成功请求数
    LOG(INFO) << "Fail count: " << fail_count;                                      // 失败请求数
    LOG(INFO) << "Elapsed time: " << elapsed.count() << " seconds";                 // 测试耗时
    LOG(INFO) << "QPS: " << (thread_count * requests_per_thread) / elapsed.count(); // 计算 QPS（每秒请求数）

    return 0;
}