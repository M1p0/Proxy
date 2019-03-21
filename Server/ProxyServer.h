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
#include <Public.h>

struct Relation
{
    string guid;   //用户的guid
    SOCKET sock;   //服务器代理连接的socket
    int64_t sockid; //客户端上 与客户端建立连接的socket
};

struct Task
{
    SOCKET dst;
    char* data;
    int Length;
};


struct Info
{
    string IP = "0.0.0.0";
    int Port = 0;
};

struct Connect_Request
{
    Info info;
    int64_t sockid;    //客户端上的sockid
    string guid;
};


class ProxyServer
{
public:
    ProxyServer();
    virtual ~ProxyServer() {};
    int Init();
    int Run();
    int Listener();
    int ClientReceiver();    //从已经验证过的用户处接收消息指令
    int ServerReceiver();    //从服务端接收消息
    int Verificator();  //验证用户
    int Connector();    //连接目标服务器
    int Forwarder();    //为目标服务器和用户转发消息
    int isVerificated(string guid);
    SOCKET GetSocket(string &guid);
    int SendDocument(Document &DocSend, SOCKET s);
    int DocToStr(Document &Document, string &str);

    CFileIO FileIO;
    string username;
    string password;
    int localport;
    int packetsize;
    MSocket sock;

    list<Connect_Request> list_connect;    //连接队列 尝试与目标服务器建立连接
    mutex mtx_list_connect;


    unordered_map<string, SOCKET> map_user;    //登陆成功且已经与目标服务器建立连接的用户(通过guid找socket)
    mutex mtx_map_user;
    list<Relation> list_user;     //guid socket(server) sockid(client)的对应关系
    mutex mtx_list_user;

    
    list<Task> list_task;   //所有需要转发的消息都存放在这
    mutex mtx_list_task;

    unordered_map<string, int(*)(const char*, string)> map_function;

private:
    SOCKET sLocal;
};
