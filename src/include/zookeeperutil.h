#pragma once

#include <semaphore.h>
#include <zookeeper/zookeeper.h>
#include <string>

// rpc服务的提供端要在rpc服务节点启动之前，把自己上面的服务在ZK注册
// 封装的zookeeper客户端类
class ZkClient
{
public:
    ZkClient();
    ~ZkClient();

    // zkclient启动连接zkserver
    void Start();

    // 在zkserver上根据指定的path创建znode节点
    void Creat(const char *path, const char *data, int datalen, int state = 0);

    // 根据参数指定的znode节点路径，或者znode节点的值
    std::string GetData(const char *path);

private:
    // zk的客户端句柄
    zhandle_t *m_zhandle;
};