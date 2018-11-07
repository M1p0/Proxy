#pragma once
#include <MSocket.h>
#include <iostream>
#include "MString.h"
#include <list>
#include <mutex>
#include <unordered_map>
class SocksProxy
{
public:
    SocksProxy(int iPort);
    int Init(int iPort);
    int Run();
    virtual ~SocksProxy() {};

private:
    struct Task
    {
        SOCKET dst;
        char* data;
        int Length;
    };
    struct Relation
    {
        SOCKET src;
        SOCKET dst;

    };
    std::list <Relation> list_relation;
    std::list <Task> list_task;
    std::list <SOCKET> list_sockets;
    std::mutex mtx_sockets;
    std::mutex mtx_relation;
    std::mutex mtx_task;
    int Listener(SOCKET sLocal);
    int Confirm();
    int Receiver();
    int Forwarder();
    MSocket sock;
    SOCKET sLocal;
};


