#include "SocksProxy.h"
#include <thread>
#include <string>
#include <Public.h>
#include "MString.h"
#include <MemPool.h>
using namespace std;

extern MemPool pool;
SocksProxy::SocksProxy(int iPort)
{
    Init(iPort);
}

int SocksProxy::Confirm()
{
    while (true)
    {
        mtx_sockets.lock();
        if (list_sockets.empty())
        {
            mtx_sockets.unlock();
            MSleep(1, "ms");
            continue;
        }
        SOCKET sClient;
        sClient = list_sockets.front();
        list_sockets.pop_front();
        mtx_sockets.unlock();
        int iconfirm_code = 0x000105;     //05 01 00
        int retVal = 0;
        char srecv_code[3];
        memset(srecv_code, 0, 3);
        retVal = sock.Recv(sClient, srecv_code, 3);
        int irecv_code = 0;
        memcpy(&irecv_code, srecv_code, 3);
        if (iconfirm_code == irecv_code)
        {
            char sreply[2] = { 0x05,0x00 };
            sock.Send(sClient, sreply, 2);

            int retVal = 0;
            string IP;
            int Port = 0;
            //     ipv4        ip          端口
            unsigned char Request_Info[4];     // 05 01 00 01  xx xx xx xx   xx xx
            unsigned char IP_Info[6];    //IP 4字节  Port 2字节
            memset(Request_Info, 0, 4);
            memset(IP_Info, 0, 6);
            retVal = sock.Recv(sClient, (char*)Request_Info, 4);
            if (Request_Info[3] == 0x01)
            {
                sock.Recv(sClient, (char*)IP_Info, 6);   //获取请求的ip地址和端口
                char sreply[10] = { 0x05,0x00,0x00,0x01 };
                for (int i = 4; i < 10; i++)
                {
                    sreply[i] = IP_Info[i - 4];
                }
                sock.Send(sClient, sreply, 10);  //05 00 00  IP Port 代理完成


                for (int i = 0; i < 4; i++)
                {
                    char temp[4];   //最大为0xFF 255 3位数  需要包括'\0'
                    memset(temp, 0, 3);
#ifdef _WIN32
                    _itoa(IP_Info[i], temp, 10);
#else
                    Mitoa((int)IP_Info[i], temp, 10);
#endif // _WIN32


                    IP = IP + temp;
                    if (i != 3)
                    {
                        IP = IP + ".";
                    }
                }


                memcpy(((char*)&Port), IP_Info + 5, 1);  //大小端转换
                memcpy(((char*)&Port) + 1, IP_Info + 4, 1);
            }
            //   请求域名     域名长度     域名        端口
            // 05 01 00 03      xx    xxxxxxxxxxx   xx xx
            else if (Request_Info[3] == 0x03)
            {
                unsigned char Length = 0;
                sock.Recv(sClient, (char*)&Length, 1);
                char Domain[256];
                memset(Domain, 0, 256);
                sock.Recv(sClient, Domain, Length);     //url
                char Temp[256];
                memset(Temp, 0, 256);
                sock.GetHostByName(Domain, Temp, 256);   //url->IP
                IP = Temp;
                memset(Temp, 0, 256);
                sock.Recv(sClient, Temp, 2);   //Port
                memcpy(((char*)&Port), Temp + 1, 1);  //大小端转换
                memcpy(((char*)&Port) + 1, Temp, 1);


                char sreply[4] = { 0x05,0x00,0x00,0x03 };
                sock.Send(sClient, sreply, 4);  //05 00 00 03 Length_domain domian Port 代理完成
                sock.Send(sClient, (char*)&Length, 1);
                sock.Send(sClient, Domain, Length);
                sock.Send(sClient, Temp, 2);
            }
            else   //IPv6
            {
                sock.Close(sClient);
                continue;
            }

            SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   //tcp

            unsigned long mode = 1;  //无阻塞socket
            ioctlsocket(sServer, FIONBIO, &mode);
            if (sock.Connect(sServer, IP.c_str(), Port) != 0)             //连接目标服务器
            {
                timeval tv = { 1,0 };  //连接超时1s
                fd_set Write;
                FD_ZERO(&Write);
                FD_SET(sServer, &Write);
                select(sServer+1, NULL, &Write, NULL, &tv);
                if (!FD_ISSET(sServer, &Write))
                {
                    sock.Close(sClient);
                    continue;
                }
                else
                {
                    mode = 0; //阻塞socket
                    ioctlsocket(sServer, FIONBIO, &mode);
                    mtx_relation.lock();
                    list_relation.push_back({ sClient,sServer });
                    list_relation.push_back({ sServer,sClient });
                    mtx_relation.unlock();
                }
            }
            else
            {
                mode = 0; //阻塞socket
                ioctlsocket(sServer, FIONBIO, &mode);
                mtx_relation.lock();
                list_relation.push_back({ sClient,sServer });
                list_relation.push_back({ sServer,sClient });
                mtx_relation.unlock();
            }
        }
        else
        {
            sock.Close(sClient);
            continue;
        }

    }

    return 0;
}

int SocksProxy::Init(int iPort)
{
    sLocal = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock.Bind(sLocal, iPort, AF_INET) < 0)
    {
        cout << "Bind Failed" << endl;
    }
    return 0;
}

int SocksProxy::Run()
{
    thread t_Confirm_1(&SocksProxy::Confirm, this);
    t_Confirm_1.detach();

    thread t_Confirm_2(&SocksProxy::Confirm, this);
    t_Confirm_2.detach();

    thread t_Confirm_3(&SocksProxy::Confirm, this);
    t_Confirm_3.detach();

    thread t_Receiver_1(&SocksProxy::Receiver, this);
    t_Receiver_1.detach();

    thread t_Receiver_2(&SocksProxy::Receiver, this);
    t_Receiver_2.detach();

    thread t_Forwarder_1(&SocksProxy::Forwarder, this);
    t_Forwarder_1.detach();

    thread t_Forwarder_2(&SocksProxy::Forwarder, this);
    t_Forwarder_2.detach();

    thread t_Listener_1(&SocksProxy::Listener, this, sLocal);
    t_Listener_1.join();

    //thread t_Listener2(&SocksProxy::Listener, this, sLocal);
    //t_Listener2.join();
    return 0;
}



int SocksProxy::Listener(SOCKET sLocal)
{
    cout << "Listener running..." << endl;
    sock.Listen(sLocal, 10);
    while (true)
    {
        SOCKET sClient = sock.Accept(sLocal);
        if (sClient == -1)
        {
            cout << "Accept failed!" << endl;
            return -1;
        }
        else
        {
            Cli_Info Info;
            sock.Getpeername(sClient, Info);
            cout << "client connected from " << Info.ip << ":" << Info.port << endl;
            mtx_sockets.lock();
            list_sockets.push_back(sClient);
            mtx_sockets.unlock();
        }
        MSleep(1, "ms");
    }
}


int SocksProxy::Receiver()
{
    while (true)
    {
        mtx_relation.lock();
        if (list_relation.empty())
        {
            mtx_relation.unlock();
            MSleep(1, "ms");
            continue;
        }
        int retVal = 0;
        Relation rel = list_relation.front();
        list_relation.pop_front();
        mtx_relation.unlock();
        SOCKET sSrc = rel.src;
        SOCKET sDst = rel.dst;
        fd_set fds_client;
        FD_ZERO(&fds_client);
        timeval tv = { 0,1 };


        const char* Zero_Bit = "";
        retVal=sock.Send(sSrc, Zero_Bit, 0);
        if (retVal < 0)
        {
            sock.Close(sSrc);
            sock.Close(sDst);
            continue;
        }

        
        if (!FD_ISSET(sSrc, &fds_client))
        {
            FD_SET(sSrc, &fds_client);
        }
        if (select(sSrc + 1, &fds_client, NULL, NULL, &tv) <= 0)
        {
            mtx_relation.lock();
            list_relation.push_back({ sSrc ,sDst });
            mtx_relation.unlock();
        }
        else
        {
            char* Data_Client = new char[10240];   // request   10kb
            memset(Data_Client, 0, 10240);
            retVal = sock.Recv(sSrc, Data_Client, 10240);
            if (retVal < 0)
            {
                list<Relation>::iterator it;
                mtx_relation.lock();
                for (it=list_relation.begin();it!=list_relation.end();)
                {
                    if (it->src==sSrc||it->dst==sSrc)
                    {
                        list_relation.erase(it++);
                    }
                    else
                    {
                        it++;
                    }
                }
                mtx_relation.unlock();
                sock.Close(sSrc);
                sock.Close(sDst);
                delete[] Data_Client;
            }
            else
            {
                Task task;
                task.data = Data_Client;
                task.dst = sDst;
                task.Length = retVal;
                mtx_task.lock();
                list_task.push_back(task);
                mtx_task.unlock();

                if (!FD_ISSET(sSrc, &fds_client))
                {
                    FD_SET(sSrc, &fds_client);
                }
                if (select(sSrc + 1, &fds_client, NULL, NULL, &tv) <= 0)  //无数据 低优先级
                {
                    mtx_relation.lock();
                    list_relation.push_back({ sSrc, sDst });
                    mtx_relation.unlock();
                }
                else //有数据 高优先级 推至链表最前部
                {
                    mtx_relation.lock();
                    list_relation.push_front({ sSrc ,sDst });
                    mtx_relation.unlock();
                }
            }
        }
        MSleep(1, "ms");
    }
    return 0;
}


int SocksProxy::Forwarder()
{
    while (true)
    {
        mtx_task.lock();
        if (list_task.empty())
        {
            mtx_task.unlock();
            MSleep(1, "ms");
            continue;
        }
        Task task = list_task.front();
        list_task.pop_front();
        mtx_task.unlock();
        int retVal=sock.Send(task.dst, task.data, task.Length);
        if (retVal<0)
        {
            sock.Close(task.dst);
        }
        delete[] task.data;
        //Destroy(task.data, pool);
        task.data = NULL;
        MSleep(1, "ms");
    }
    return 0;
}