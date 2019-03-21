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
    string guid;   //�û���guid
    SOCKET sock;   //�������������ӵ�socket
    int64_t sockid; //�ͻ����� ��ͻ��˽������ӵ�socket
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
    int64_t sockid;    //�ͻ����ϵ�sockid
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
    int ClientReceiver();    //���Ѿ���֤�����û���������Ϣָ��
    int ServerReceiver();    //�ӷ���˽�����Ϣ
    int Verificator();  //��֤�û�
    int Connector();    //����Ŀ�������
    int Forwarder();    //ΪĿ����������û�ת����Ϣ
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

    list<Connect_Request> list_connect;    //���Ӷ��� ������Ŀ���������������
    mutex mtx_list_connect;


    unordered_map<string, SOCKET> map_user;    //��½�ɹ����Ѿ���Ŀ��������������ӵ��û�(ͨ��guid��socket)
    mutex mtx_map_user;
    list<Relation> list_user;     //guid socket(server) sockid(client)�Ķ�Ӧ��ϵ
    mutex mtx_list_user;

    
    list<Task> list_task;   //������Ҫת������Ϣ���������
    mutex mtx_list_task;

    unordered_map<string, int(*)(const char*, string)> map_function;

private:
    SOCKET sLocal;
};
