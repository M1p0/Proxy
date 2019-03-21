#pragma once
#include <MSocket.h>
#include "Utils.h"
#include <iostream>
#include "MString.h"
#include <list>
#include <mutex>
#include <unordered_map>
#include "MJson.h"
#include "CFileIO.h"

struct Info
{
    string IP="0.0.0.0";
    int Port=0;
};

struct Task
{
    SOCKET dst;
    char* data;
    int Length;
};


class ProxyClient
{
public:
    ProxyClient();
    int Init();
    virtual ~ProxyClient() {};
    int Run();
    int Listener(SOCKET sLocal);
    int Confirm();
    int ClientReceiver();
    int Forwarder();
    int SendDocument(Document &DocSend, SOCKET s);
    int Login();
    std::list <SOCKET> list_verification;
    std::mutex mtx_list_verification;

    std::list <SOCKET> list_request;
    std::mutex mtx_list_request;

    unordered_map<SOCKET, Info> map_Info;
    std::mutex mtx_map_Info;

    std::list<Task> list_task;
    std::mutex mtx_list_task;

    unordered_map<string, int(*)(const char*)> map_function;

    string Guid;
    string username;
    string password;
    int localport;
    string serverip;
    int serverport;
    int packetsize;

    CFileIO FileIO;
    MSocket sock;
    SOCKET sLocal;
    SOCKET sServer;

};