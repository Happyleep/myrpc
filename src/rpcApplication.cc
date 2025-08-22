#include "rpcApplication.h"
#include <cstdlib>
#include <unistd.h>
#include <iostream>

rpcConfig rpcApplication::m_config;                      // 全局配置对象
std::mutex rpcApplication::m_mutex;                      // 用于线程安全的互斥锁
rpcApplication *rpcApplication::m_application = nullptr; // 单列对象指针，初始为空

// 初始化函数 用于解析命令行参数并加载配置
void rpcApplication::Init(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cout << "格式:command - i < 配置文件路径" << std::endl;
        exit(EXIT_FAILURE);
    }

    int o;
    std::string config_file;
    // 使用getopt解析命令行参数，-i表示指定配置文件
    while ((o = getopt(argc, argv, "i:")) != -1)
    {
        switch (o)
        {
        case 'i':                 // 如果参数是-i，后面的值就是配置文件的路径
            config_file = optarg; // 将配置文件路径保存到config_file
            break;
        case '?': // 如果出现未知参数（不是-i），提示正确格式并退出
            std::cout << "格式: command -i <配置文件路径>" << std::endl;
            exit(EXIT_FAILURE);
            break;
        case ':': // 如果-i后面没有跟参数，提示正确格式并退出
            std::cout << "格式: command -i <配置文件路径>" << std::endl;
            exit(EXIT_FAILURE);
            break;
        default:
            break;
        }
    }

    // 加载配置文件  字符串的格式转换
    m_config.LoadConfigFile(config_file.c_str());

    std::cout << "rpcserverip: " << m_config.Load("rpcserviceip") << std::endl;
    std::cout << "rpcserverport: " << m_config.Load("rpcserviceport") << std::endl;
    std::cout << "zookeeperip: " << m_config.Load("zookeeperip") << std::endl;
    std::cout << "zookeeperport: " << m_config.Load("zookeeperport") << std::endl;
}

// 获取单例对象的引用，保证全局只有一个实例
rpcApplication &rpcApplication::GetInstance()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_application == nullptr)
    {
        m_application = new rpcApplication(); // 创建单例对象
        atexit(deleteInstance);               // 注册atexit函数，程序退出时自动销毁单例对象 标准库函数
    }
    return *m_application; // 返回单例对象的引用
}

// 程序退出时自动调用的函数，用于销毁单例对象
void rpcApplication::deleteInstance()
{
    if (m_application)
        delete m_application;
}

rpcConfig &rpcApplication::GetConfig()
{
    return m_config;
}