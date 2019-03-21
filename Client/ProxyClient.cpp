#include "ProxyClient.h"
#include <thread>
#include <string>
#include <Public.h>
#include "MString.h"
#include <MemPool.h>
#include "functions.h"
#include <Public.h>
using namespace std;

int ProxyClient::SendDocument(Document &DocSend, SOCKET s)
{
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    DocSend.Accept(writer);
    string JsonSend = buffer.GetString();
    int Length = JsonSend.size();
    sock.Send(s, (char*)&Length, 4);
    sock.Send(s, JsonSend.c_str(), Length);
    return 0;
}

int ProxyClient::Login()
{
    int retVal;
    Packet pack;
    //登录
    Document DocSend;
    DocSend.SetObject();
    Value vusername;
    Value vpassword;
    vusername.SetString(username.c_str(), username.size(), DocSend.GetAllocator());
    vpassword.SetString(password.c_str(), password.size(), DocSend.GetAllocator());
    DocSend.AddMember("cmd", "login", DocSend.GetAllocator());
    DocSend.AddMember("username", vusername, DocSend.GetAllocator());
    DocSend.AddMember("password", vpassword, DocSend.GetAllocator());
    SendDocument(DocSend, sServer);
    //获取guid
    decltype(pack.Length) Length = 0;
    char *buf = new char[packetsize];
    memset(buf, 0, packetsize);
    retVal = sock.Recv(sServer, (char*)&Length, sizeof(Length));
    retVal = sock.Recv(sServer, (char*)buf, Length);

    Document DocRecv;
    DocRecv.Parse(buf);
    if (DocRecv.IsObject())
    {
        if (DocRecv.HasMember("status"))
        {
            Value &vstatus = DocRecv["status"];
            string status = vstatus.GetString();
            if (strcmp(status.c_str(), "fail") == 0)
            {
                return -1;
            }
        }
        else
        {
            return -1;
        }
        if (DocRecv.HasMember("guid"))
        {
            Value &vguid = DocRecv["guid"];
            Guid = vguid.GetString();
            cout << "login success" << endl;
            cout << "get guid:" << Guid << endl;
        }
    }
    else
    {
        return -1;
    }
    return 0;
}

ProxyClient::ProxyClient()
{
    Init();
    InitFunc();
}

int ProxyClient::Init()
{
    //初始化用户配置
    uint64_t config_size = 1024 * 1024;
    char* config = new char[config_size];    //配置文件最大1mb
    FileIO.Read("./config_client.json", config, 0, config_size);
    Document document;
    document.SetObject();
    document.Parse(config);
    delete[] config;

    if (document.IsObject())
    {
        if (document.HasMember("username"))
        {
            Value &vusername = document["username"];
            this->username = vusername.GetString();
        }
        else
        {
            cout << "no username check config.json" << endl;
            return -1;
        }
        if (document.HasMember("password"))
        {
            Value &vpassword = document["password"];
            this->password = vpassword.GetString();
        }
        else
        {
            cout << "no password check config.json" << endl;
            return -1;
        }
        if (document.HasMember("localport"))
        {
            Value &vlocalport = document["localport"];
            this->localport = vlocalport.GetInt();
        }
        else
        {
            cout << "no localport check config.json" << endl;
            return -1;
        }
        if (document.HasMember("serverport"))
        {
            Value &vserverport = document["serverport"];
            this->serverport = vserverport.GetInt();
        }
        else
        {
            cout << "no serverport check config.json" << endl;
            return -1;
        }
        if (document.HasMember("serverip"))
        {
            Value &vserverip = document["serverip"];
            this->serverip = vserverip.GetString();
        }
        else
        {
            cout << "no serverip check config.json" << endl;
            return -1;
        }

        if (document.HasMember("packetsize"))
        {
            Value &vpacketsize = document["packetsize"];
            this->packetsize = vpacketsize.GetUint64()+1024;
        }
        else
        {
            packetsize = 102400+1024;    //default packetsize=102400  100kb
        }

    }
    else
    {
        cout << R"(Wrong Json!Check "./config.json")" << endl;
        return -1;
    }

    //初始化本地监听端口
    sLocal = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock.Bind(sLocal, localport, AF_INET) < 0)
    {
        cout << "Bind Failed" << endl;
        return -1;
    }

    sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock.Connect(sServer, serverip.c_str(), serverport) < 0)
    {
        cout << "connect server failed" << endl;
        return -1;
    }
    InitFunc();
    return 0;
}

int ProxyClient::Run()
{
    if (Login() != 0)
    {
        cout << "login failed" << endl;
        return -1;
    }

    thread t_Listener_1(&ProxyClient::Listener, this, sLocal);
    t_Listener_1.detach();
    thread t_Receiver_1(&ProxyClient::ClientReceiver, this);
    t_Receiver_1.detach();
    thread t_Confirm_1(&ProxyClient::Confirm, this);
    t_Confirm_1.detach();
    thread t_Forwarder_1(&ProxyClient::Forwarder, this);
    t_Forwarder_1.join();
    return 0;
}

int ProxyClient::Listener(SOCKET sLocal)
{
    int retVal;
    cout << "Listener running..." << endl;
    retVal = sock.Listen(sLocal, 10);
    if (retVal != 0)
    {
        cout << "Listen failed" << endl;
        return -1;
    }
    while (true)
    {
        SOCKET sClient = sock.Accept(sLocal);
        if (sClient == -1)
        {
            //cout << "Accept failed!" << endl;
            continue;
        }
        else
        {
            //Cli_Info Info;
            //sock.Getpeername(sClient, Info);
            //cout << "client connected from " << Info.ip << ":" << Info.port << endl;
            mtx_list_verification.lock();
            list_verification.push_back(sClient);
            mtx_list_verification.unlock();
        }
        MSleep(1, "ms");
    }
    return 0;
}

int ProxyClient::Confirm()
{
    while (true)
    {
        mtx_list_verification.lock();
        if (list_verification.empty())
        {
            mtx_list_verification.unlock();
            MSleep(1, "ms");
            continue;
        }
        SOCKET sClient;
        sClient = list_verification.front();
        list_verification.pop_front();
        mtx_list_verification.unlock();
        int iconfirm_code = 0x000105;     //05 01 00
        int retVal = 0;
        char srecv_code[3];
        memset(srecv_code, 0, 3);
        retVal = sock.Recv(sClient, srecv_code, 3);
        int irecv_code = 0;
        memcpy(&irecv_code, srecv_code, 3);
        if (iconfirm_code == irecv_code)
        {
            Document DocSend;
            DocSend.SetObject();
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
                Value vIP;
                Value vGuid;
                vIP.SetString(IP.c_str(), IP.size(), DocSend.GetAllocator());
                vGuid.SetString(Guid.c_str(), Guid.size(), DocSend.GetAllocator());
                DocSend.AddMember("cmd", "connect", DocSend.GetAllocator());
                DocSend.AddMember("ip", vIP, DocSend.GetAllocator());
                DocSend.AddMember("port", Port, DocSend.GetAllocator());
                DocSend.AddMember("guid", vGuid, DocSend.GetAllocator());
                DocSend.AddMember("sockid", (int64_t)sClient, DocSend.GetAllocator());
                SendDocument(DocSend, sServer);
                mtx_map_Info.lock();
                map_Info.insert(pair<SOCKET, Info>(sClient, { IP,Port }));
                mtx_map_Info.unlock();
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


                Value vIP;
                Value vGuid;
                vIP.SetString(IP.c_str(), IP.size(), DocSend.GetAllocator());
                vGuid.SetString(Guid.c_str(), Guid.size(), DocSend.GetAllocator());
                DocSend.AddMember("cmd", "connect", DocSend.GetAllocator());
                DocSend.AddMember("ip", vIP, DocSend.GetAllocator());
                DocSend.AddMember("port", Port, DocSend.GetAllocator());
                DocSend.AddMember("guid", vGuid, DocSend.GetAllocator());
                DocSend.AddMember("sockid", (int64_t)sClient, DocSend.GetAllocator());
                SendDocument(DocSend, sServer);
                mtx_map_Info.lock();
                map_Info.insert(pair<SOCKET, Info>(sClient, { IP,Port }));
                mtx_map_Info.unlock();
            }
            else   //IPv6
            {
                mtx_map_Info.lock();
                map_Info.erase(sClient);
                mtx_map_Info.unlock();
                sock.Close(sClient);
                continue;
            }
        }
        else
        {
            mtx_map_Info.lock();
            map_Info.erase(sClient);
            mtx_map_Info.unlock();
            sock.Close(sClient);
            continue;
        }

    }

    return 0;
}

int ProxyClient::ClientReceiver()   //接收来自服务端的数据
{
    while (true)
    {
        int retVal = 0;
        int Length;
        char* Data_recv = new char[packetsize];
        memset(Data_recv, 0, packetsize);
        retVal = sock.Recv(sServer, (char*)&Length, 4);
        retVal = sock.Recv(sServer, Data_recv, Length);
        if (retVal < 0)
        {
            cout << "disconnect from proxyserver" << endl;
            sock.Close(sServer);
            delete[] Data_recv;
            break;
        }
        else
        {
            Document DocRecv;
            DocRecv.Parse(Data_recv);
            if (DocRecv.IsObject())
            {
                if (DocRecv.HasMember("cmd"))
                {
                    Value &vcmd = DocRecv["cmd"];
                    string cmd = vcmd.GetString();
                    unordered_map<string, int(*)(const char*)>::iterator it;
                    it = map_function.find(cmd);
                    if (it != map_function.end())
                    {
                        it->second(Data_recv);
                    }
                    else  //不支持的cmd
                    {

                    }
                }
                else //无cmd
                {

                }
            }
            else  //不是json
            {

            }
        }
        MSleep(1, "ms");
    }
    return 0;
}

int ProxyClient::Forwarder()    //从被代理程序接收数据包并转发给server
{
    while (true)
    {
        SOCKET sClient;
        int retVal;
        mtx_list_request.lock();
        if (list_request.empty())
        {
            mtx_list_request.unlock();
            MSleep(1, "ms");
            continue;
        }
        sClient = list_request.front();
        list_request.pop_front();
        mtx_list_request.unlock();
        const char* Zero_Bit = "";
        retVal = sock.Send(sClient, Zero_Bit, 0);
        if (retVal<0)
        {
            mtx_map_Info.lock();
            map_Info.erase(sClient);
            mtx_map_Info.unlock();
            sock.Close(sClient);
            continue;
        }
        fd_set fds_client;
        FD_ZERO(&fds_client);
        timeval tv = { 0,0 };
        FD_SET(sClient, &fds_client);
        if (select(sClient+1,&fds_client,NULL,NULL,&tv)<=0)
        {
            mtx_list_request.lock();
            list_request.push_back(sClient);
            mtx_list_request.unlock();
        }
        else
        {
            char* Data_Client = new char[packetsize];
            memset(Data_Client, 0, packetsize);
            retVal = sock.Recv(sClient, Data_Client, packetsize);
            if (retVal<0)
            {
                mtx_map_Info.lock();
                map_Info.erase(sClient);
                mtx_map_Info.unlock();
                sock.Close(sClient);
                delete[] Data_Client;
            }
            else
            {
                string sData = Data_Client;
                Document DocSend;
                DocSend.SetObject();
                Value vguid;
                Value vdata;
                vguid.SetString(Guid.c_str(), DocSend.GetAllocator());
                vdata.SetString(sData.c_str(), DocSend.GetAllocator());
                DocSend.AddMember("cmd", "forward", DocSend.GetAllocator());
                DocSend.AddMember("guid", vguid, DocSend.GetAllocator());
                DocSend.AddMember("length", retVal, DocSend.GetAllocator());
                DocSend.AddMember("sockid", (int64_t)sClient, DocSend.GetAllocator());
                DocSend.AddMember("data", vdata, DocSend.GetAllocator());
                SendDocument(DocSend, sServer);
                delete[] Data_Client;

                if (!FD_ISSET(sClient, &fds_client))
                {
                    FD_SET(sClient, &fds_client);
                }
                if (select(sClient + 1, &fds_client, NULL, NULL, &tv) <= 0)  //无数据 低优先级
                {
                    mtx_list_request.lock();
                    list_request.push_back(sClient);
                    mtx_list_request.unlock();
                }
                else //有数据 高优先级 推至链表最前部   若socket中一直有数据 可能导致其他socket数据堆积  启动多个Forward线程可以缓解问题
                {
                    mtx_list_request.lock();
                    list_request.push_front(sClient);
                    mtx_list_request.unlock();
                }
            }
        }
        MSleep(1, "ms");
    }

    return 0;
}
