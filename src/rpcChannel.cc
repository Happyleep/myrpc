#include "rpcChannel.h"
#include "rpcHeader.pb.h"
#include "zookeeperutil.h"
#include "rpcApplication.h"
#include "rpcController.h"
#include "memory"
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <rpcLogger.h>

/*
header_size + service_name method_name args_size + args
*/
std::mutex g_data_mutx; // 全局互斥锁，用于保护共享数据的线程安全

// 框架提供给rpc调用方的接口，用于发送rpc请求
// 集中来做所有rpc方法调用的参数序列化和网络发送
void rpcChannel::CallMethod(const ::google::protobuf::MethodDescriptor *method,
                            ::google::protobuf::RpcController *controller,
                            const ::google::protobuf::Message *request,
                            ::google::protobuf::Message *response,
                            ::google::protobuf::Closure *done)
{
    if (-1 == m_clientfd) // 如果客户端socket未初始化
    {
        // 获取服务对象名和方法名
        const google::protobuf::ServiceDescriptor *sd = method->service();
        service_name = sd->name();
        method_name = method->name();

        // 客户端需要查询zookeeper，找到提供该服务的服务器地址
        ZkClient zkCli;
        zkCli.Start(); // 连接ZooKeeper服务器
        std::string host_data = QueryServiceHost(&zkCli, service_name, method_name, m_idx);
        m_ip = host_data.substr(0, m_idx); // 从查询结果中提取IP地址
        std::cout << "ip" << m_ip << std::endl;
        m_port = atoi(host_data.substr(m_idx + 1, (host_data.size() - m_idx)).c_str());
        std::cout << "port" << m_port << std::endl;

        // 尝试连接服务器
        auto rt = newConnect(m_ip.c_str(), m_port);
        if (!rt)
        {
            LOG(ERROR) << "connect server error ";
            return;
        }
        else
        {
            LOG(INFO) << "connect server success ";
        }
    }

    // 将请求的参数序列化为字符串，并计算其长度
    uint32_t args_size{};
    std::string args_str;
    if (request->SerializeToString(&args_str)) // 序列化请求参数
    {
        args_size = args_str.size();
    }
    else{
        controller->SetFailed("serialize request fail");
        return;
    }

    // 定义rpc请求的头部信息
    rpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    // 将rpc头部信息序列化为字符串，并计算器长度
    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    // 将头部长度和头部长度拼接成完整的rpc请求报文
    std::string send_rpc_str;
    {
        /*StringOutputStream 是 Protobuf 提供的一个包装类，它可以把编码结果写入到 std::string 中。
        传入 &send_rpc_str，意味着所有写入的内容都会追加到这个字符串末尾。*/
        google::protobuf::io::StringOutputStream string_output(&send_rpc_str);
        google::protobuf::io::CodedOutputStream coded_output(&string_output);
        coded_output.WriteVarint32(static_cast<uint32_t>(header_size)); // 写入头部长度
        coded_output.WriteString(rpc_header_str); // 写入头部信息
    }
    send_rpc_str += args_str; // 拼接请求参数

    // 发送rpc请求到服务器
    if (-1 == send(m_clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(m_clientfd);
        char errtxt[512] = {0};
        std::cout << "send error " << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        controller->SetFailed(errtxt); // 设置错误信息
        return;
    }

    // 接受服务器的响应
    char recv_buf[1024];
    int recv_size = 0;
    if (-1 == (recv_size = recv(m_clientfd, recv_buf, 1024, 0)))
    {
        char errtxt[512] = {};
        std::cout << "recv error" << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        controller->SetFailed(errtxt);
        return;
    }

    // 将接收到的响应数据反序列化为response对象
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(m_clientfd); // 反序列化失败，关闭socket
        char errtxt[512] = {0};
        std::cout << "parse error " << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        controller->SetFailed(errtxt); // 设置错误信息
        return;
    }
    close(m_clientfd);
}


// 创建新的socket连接
bool rpcChannel::newConnect(const char *ip, uint16_t port)
{
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        std::cout << "socket error " << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        LOG(ERROR) << "socket error: " << errtxt;
        return false;
    }
    // 设置服务器地址信息
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    // 尝试连接服务器
    if (-1 == connect(clientfd, (struct sockaddr *)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        std::cout << "connect error " << strerror_r(errno, errtxt, sizeof(errtxt)) << std::endl;
        LOG(ERROR) << "connect error: " << errtxt;
        return false;
    }
    m_clientfd = clientfd;
    return true;
}

// 从ZooKeeper查询服务地址
std::string rpcChannel::QueryServiceHost(ZkClient *zkclient, std::string service_name,
                                         std::string method_name, int &idx)
{
    std::string method_path = "/" + service_name + "/" + method_name;
    std::cout << "method_path: " << method_path << std::endl;
    std::unique_lock<std::mutex> lock(g_data_mutx);
    std::string host_data_1 = zkclient->GetData(method_path.c_str());
    lock.unlock();

    if (host_data_1 == "")
    {
        LOG(ERROR) << method_path + "is not exist!";
        return " ";
    }

    idx = host_data_1.find(":"); // 查找IP和port的分隔符
    if (idx == -1)
    {
        LOG(ERROR) << method_path + "address is invalid!";
        return " ";
    }
    
    return host_data_1; // 返回服务器地址
}
// 构造函数，支持延迟连接
rpcChannel::rpcChannel(bool connectNow) : m_clientfd(-1), m_idx(0) {
    if (!connectNow) {  // 如果不需要立即连接
        return;
    }
    // 尝试连接服务器，最多重试3次
    auto rt = newConnect(m_ip.c_str(), m_port);
    int count = 3;  // 重试次数
    while (!rt && count--) {
        rt = newConnect(m_ip.c_str(), m_port);
    }
}