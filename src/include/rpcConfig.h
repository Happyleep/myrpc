#pragma once
#include <unordered_map>
#include <string>


class rpcConfig
{
public:
    void LoadConfigFile(const char* config_file); //加载配置文件
    std::string Load(const std::string &key); //查找key对应的value
private:
    std::unordered_map<std::string, std::string> config_map;
    void Trim(std::string &read_buff);// 去掉字符串前后的空格
};


// 让服务端和客户端不需要硬编码参数，而是从配置文件读取运行参数。
// 提供一个配置文件读取器，
// 从配置文件中解析 key=value 的配置项，并提供按键查询功能。
// 服务器地址、端口号、注册中心信息等配置的加载。