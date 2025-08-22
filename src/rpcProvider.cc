#include "rpcProvider.h"
#include "rpcApplication.h"
#include "rpcHeader.pb.h"
#include "rpcLogger.h"
#include <iostream>

// 将UserService服务及其中的方法Login发布出去，供远端调用。
// 注意我们的UserService是继承自UserServiceRpc的。
// 远端想要请求UserServiceRpc服务其实请求的就是UserService服务。
// 而UserServiceRpc只是一个虚类而已

// 注册服务对象及其方法，以便服务端能够处理客户端的RPC请求
void rpcProvider::NotifyService(google::protobuf::Service *service)
{
    // 服务端需要知道客户端想要调用的服务对象和方法，
    // 这些信息会保存在一个数据结构（如 ServiceInfo）中。
    ServiceInfo serviceInfo;

    // 参数类型设置为 google::protobuf::Service，是因为所有由 protobuf 生成的服务类
    // 都继承自 google::protobuf::Service，这样我们可以通过基类指针指向子类对象，
    // 实现动态多态

    // 通过动态多态调用 service->GetDescriptor()，
    // GetDescriptor()方法会返回protobuf生成的服务对象的描述信息(ServiceDescriptor)
    const google::protobuf::ServiceDescriptor *psd = service->GetDescriptor();

    // 通过 ServiceDescriptor，我们可以获取该服务类中定义的方法列表，
    // 并进行相应的注册和管理。

    // 获取服务的名字
    std::string serviceName = psd->name();
    // 获取服务端对象service的方法数量
    int methodCount = psd->method_count(); // 定义多少个远端能调用的函数

    // 打印服务名
    std::cout << "ServiceName=" << serviceName << std::endl; //(UserServiceRpc)

    // 遍历服务中的所有方法，并注册到服务信息中
    for (int i = 0; i < methodCount; ++i)
    {
        // 获取服务中的方法描述
        const google::protobuf::MethodDescriptor *pmd = psd->method(i);
        std::string methodName = pmd->name();
        std::cout << "methodName = " << methodName << std::endl;
        // emplace 比 insert 略高效，直接构造键值对
        serviceInfo.methodMap.emplace(methodName, pmd); // 将方法名和方法描述符存入map
    }
    serviceInfo.service = service;                // 保存服务对象
    serviceMap.emplace(serviceName, serviceInfo); // 将服务信息存入服务map
}

// 启动RPC服务节点，开始提供远程网络调用服务
// 将待发布的服务对象及其方法发布到ZooKeeper上，
// 同时利用Muduo库提供的网络模块开启对RpcServer的(IP, Port)的监听
void rpcProvider::run()
{
    // 读取配置文件中的RPC服务器IP和端口
    // atoi把参数 str 所指向的字符串转换为一个整数（类型为 int 型）。
    std::string ip = rpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    uint16_t port = atoi(rpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());

    // 使用muduo网络库，创建地址对象
    muduo::net::InetAddress address(ip, port);

    // 创建TcpServer对象
    std::shared_ptr<muduo::net::TcpServer> server = std::make_shared<muduo::net::TcpServer>(&eventLoop, address, "rpcProvider");

    // 绑定连接回调和消息回调，分离网络连接业务和消息处理业务
    server->setConnectionCallback(std::bind(&rpcProvider::OnConnection, this, std::placeholders::_1));
    server->setMessageCallback(std::bind(&rpcProvider::OnMessage, this, std::placeholders::_1,
                                         std::placeholders::_2, std::placeholders::_3));

    // 设置muduo库的线程数量
    server->setThreadNum(4);

    // 将当前RPC节点上要发布的服务全部注册到ZooKeeper上，让RPC客户端cller可以在ZooKeeper上发现服务
    // session timeout 30s   zkclient API (zookeeper_init专门启个线程做网络IO操作)   网络I/O线程  1/3 timeout时间发送ping消息
    ZkClient zkclient; // 实例化一个ZooKeeper对象
    zkclient.Start();  // 连接ZooKeeper服务器

    // service_name为永久节点，method_name为临时节点
    for (auto &sp : serviceMap)
    {
        // service_name 在ZooKeeper中的目录是"/"+service_name
        std::string service_path = "/" + sp.first;
        zkclient.Creat(service_path.c_str(), nullptr, 0); // 创建服务节点
        for (auto &mp : sp.second.methodMap)
        {

            std::string method_path = service_path + "/" + mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data, "%s:%d", ip.c_str(), port); // 将IP和端口信息存入节点数据
            // ZOO_EPHEMERAL表示这个节点是临时节点，在客户端断开连接后，ZooKeeper会自动删除这个节点
            zkclient.Creat(method_path.c_str(), method_path_data, strlen(method_path_data), ZOO_EPHEMERAL);
        }
    }
    // RPC服务端准备启动，打印信息
    std::cout << "RpcProvider strat service at ip: " << ip << "port : " << port << std::endl;

    // 启动网络服务
    server->start();
    eventLoop.loop(); // 进入事件循环
}

// 连接回调函数，处理客户端连接事件 新的socket连接回调
void rpcProvider::OnConnection(const muduo::net::TcpConnectionPtr &conn)
{
    if (!conn->connected())
    {
        // 和rpc client的连接断开了
        // 如果连接关闭，则断开连接 和rpc client的连接断开了
        conn->shutdown();
    }
}
/*
在框架内部，rpcProvider和rpcConsumer协商好通信用的protobuf数据类型
service_name method_name args(args_size);定义proto的message类型，进行数据的序列化和反序列化
consumer提供的UserServiceLogin zhang san123456  无法区分(Tcp粘包问题)
需要特定格式的大小的长度
header_size(4字节：服务名和方法名) + header_str + args_str

*/

// 已建立连接用户的读写事件回调，如果远端有一个rpc服务请求，那么message就会响应
void rpcProvider::OnMessage(const muduo::net::TcpConnectionPtr &conn,
                            muduo::net::Buffer *buffer,
                            muduo::Timestamp receive_time)
{
    // 1. 网络上接收的远程rpc调用请求的字符流    Login args 方法名和参数
    std::string recv_buf = buffer->retrieveAllAsString();
    // 2. 对收到的字符流反序列化

    // 使用 Protobuf 提供的 ArrayInputStream 包装该字符串数据，作为底层输入源。
    google::protobuf::io::ArrayInputStream raw_input(recv_buf.data(), recv_buf.size());
    // 基于 raw_input 构造一个 CodedInputStream，可以方便地读取 Protobuf 编码的 Varint 等格式数据。
    google::protobuf::io::CodedInputStream conded_input(&raw_input);

    // 读取变长整数，获得 RpcHeader 的字节长度，接下来读取这么多字节作为 header。
    uint32_t header_size{};
    conded_input.ReadVarint32(&header_size); // 解析header_size

    // 3. 从反序列化的字符流中解析出service_name 和 method_name 和 args_str参数
    std::string rcp_header_str;
    rpc::RpcHeader rpcHeader; //

    std::string service_name;
    std::string method_name;
    uint32_t args_size{};

    // 设置读取限制
    google::protobuf::io::CodedInputStream::Limit msg_limit = conded_input.PushLimit(header_size);
    conded_input.ReadString(&rcp_header_str, header_size);
    // 恢复之前的限制，以便安全地继续读取其他数据
    conded_input.PopLimit(msg_limit);

    if (rpcHeader.ParseFromString(rcp_header_str))
    {
        service_name = rpcHeader.service_name();
        method_name = rpcHeader.method_name();
        args_size = rpcHeader.args_size();
    }
    else
    {
        rpcLogger::ERROR("rpcHeader parse error");
        return;
    }

    std::string args_str; // rpc参数
    // 直接读取args_size长度的字符串数据
    bool read_args_success = conded_input.ReadString(&args_str, args_size);
    if (!read_args_success)
    {
        rpcLogger::ERROR("read args error");
        return;
    }

    // 打印调试信息
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "header_size: " << header_size << std::endl;
    std::cout << "rpc_head_str: " << rcp_header_str << std::endl;
    std::cout << "service_name: " << service_name << std::endl;
    std::cout << "method_name: " << method_name << std::endl;
    std::cout << "args_size: " << args_size << std::endl;
    std::cout << "args_str: " << args_str << std::endl;
    std::cout << "----------------------------------------" << std::endl;


    // 4. serviceMap是一个哈希表，我们之前将服务对象和方法对象保存在这个表里面，
    // 根据service_name和method_name可以从serviceMap中找到服务对象service和方法对象描述符method。
    auto it = serviceMap.find(service_name);
    if (it == serviceMap.end())
    {
        std::cout << service_name << "is not exist" << std::endl;
        return;
    }
    auto mit = it->second.methodMap.find(method_name);
    if (mit == it->second.methodMap.end())
    {
        std::cout << service_name << "." << method_name << "is not exist " << std::endl;
        return;
    }
    google::protobuf::Service *service = it->second.service;        // 获取service对象 new UserService
    const google::protobuf::MethodDescriptor *method = mit->second; // 获取method对象
    // 5. 将args_str解码至request对象中
    // 之后request对象就可以这样使用了：request.name() = "zhangsan"  |  request.pwd() = "123456"

    // 生成RPC方法调用请求的request和响应的response参数

    //  动态创建请求对象
    google::protobuf::Message *request = service->GetRequestPrototype(method).New();
    // 请求方法所需要的参数，反序列化解出结果
    if (!request->ParseFromString(args_str))
    {
        std::cout << service_name << ":" << method_name << "prase error" << std::endl;
        return;
    }

    // 动态创建响应对象
    google::protobuf::Message *response = service->GetResponsePrototype(method).New();

    // 给下面的method方法的调用，绑定一个Closuer回调函数，用于在方法调用完成后发送响应
    google::protobuf::Closure *done = google::protobuf::NewCallback<rpcProvider,
                                                                    const muduo::net::TcpConnectionPtr &,
                                                                    google::protobuf::Message *>(this,
                                                                                                 &rpcProvider::SendRpcResponse, conn, response);
    // 在框架上根据远端RPC请求，调用当前RPC节点上发布的方法
    service->CallMethod(method, nullptr, request, response, done); // 调用服务方法
}

// 发送RPC响应给客户端
// Closure的回调操作，用于序列化rpc的响应和网络发送
void rpcProvider::SendRpcResponse(const muduo::net::TcpConnectionPtr &conn, google::protobuf::Message *response)
{
    std::string response_str;
    if (response->SerializeToString(&response_str))
    {
        // 序列化成功，通过网络把RPC方法执行的结果返回给RPC调用方   发送响应给rpc client也就是CallMethod方法
        conn->send(response_str);
    }
    else
    {
        std::cout << "serialize error!" << std::endl;
    }
    conn->shutdown();
}

// 析构函数，退出事件循环
rpcProvider::~rpcProvider()
{
    std::cout << "~rpcProvider()" << std::endl;
    eventLoop.quit();
}