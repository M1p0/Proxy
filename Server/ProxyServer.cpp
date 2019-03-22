#include "ProxyServer.h"
#include "functions.h"

ProxyServer::ProxyServer()
{
    Init();
}


int ProxyServer::SendDocument(Document &DocSend, SOCKET s)
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

int ProxyServer::DocToStr(Document &Document, string & str)
{
    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    Document.Accept(writer);
    str = buffer.GetString();
    return 0;
}


int ProxyServer::isVerificated(string guid)
{
    mtx_map_user.lock();
    unordered_map<string, SOCKET>::iterator it;
    it = map_user.find(guid);
    if (it != map_user.end())
    {
        mtx_map_user.unlock();
        return 1;
    }
    else
    {
        mtx_map_user.unlock();
        return -1;
    }
}

SOCKET ProxyServer::GetSocket(string &guid)
{
    mtx_map_user.lock();
    unordered_map<string, SOCKET>::iterator it;
    it = map_user.find(guid);
    SOCKET sClient = it->second;
    mtx_map_user.unlock();
    return sClient;

}

int ProxyServer::Init()
{
    uint64_t config_size = 1024 * 1024;
    char* config = new char[config_size];
    FileIO.Read("./config_server.json", config, 0, config_size);
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
            cout << "no serverport check config.json" << endl;
            return -1;
        }
        if (document.HasMember("packetsize"))
        {
            Value &vpacketsize = document["packetsize"];
            this->packetsize = vpacketsize.GetInt() + 1024;
        }
        else
        {
            packetsize = 102400 + 1024;    //default packetsize=102400  100kb
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
    InitFunc();
    return 0;

}

int ProxyServer::Run()
{
    thread t_Listener_1(&ProxyServer::Listener, this);
    t_Listener_1.detach();


    thread t_Verificator_1(&ProxyServer::Verificator, this);
    t_Verificator_1.detach();

    thread t_ClientReceiver_1(&ProxyServer::ClientReceiver, this);
    t_ClientReceiver_1.detach();


    thread t_ServerReceiver_1(&ProxyServer::ServerReceiver, this);
    t_ServerReceiver_1.detach();

    thread t_Connector_1(&ProxyServer::Connector, this);
    t_Connector_1.detach();

    thread t_Forwarder_1(&ProxyServer::Forwarder, this);
    t_Forwarder_1.join();

    return 0;
}

int ProxyServer::Listener()
{
    cout << "Listerner running.." << endl;
    int retVal = 0;
    retVal = sock.Listen(sLocal, 10);
    if (retVal == -1)
    {
        sock.Close(sLocal);
        cout << "Listen failed" << endl;
        return -1;
    }
    return 0;
}

int ProxyServer::ClientReceiver()
{
    while (true)
    {
        int retVal = 0;
        mtx_list_user.lock();
        if (list_user.empty())
        {
            mtx_list_user.unlock();
            MSleep(1, "ms");
            continue;
        }
        Relation rel;
        rel = list_user.front();
        list_user.pop_front();
        mtx_list_user.unlock();

        SOCKET sClient = GetSocket(rel.guid);
        fd_set fds_client;
        FD_ZERO(&fds_client);
        timeval tv = { 0,1 };


        const char* Zero_Bit = "";
        retVal = sock.Send(sClient, Zero_Bit, 0);       //发送0字节用于判断socket是否连接正常
        if (retVal < 0)   //客户端已断开连接
        {
            sock.Close(sClient);
            mtx_map_user.lock();
            map_user.erase(rel.guid);
            mtx_map_user.unlock();
            continue;
        }
        if (!FD_ISSET(sClient, &fds_client))
        {
            FD_SET(sClient, &fds_client);
        }
        if (select(sClient + 1, &fds_client, NULL, NULL, &tv) <= 0)   //连接正常 但客户端无数据请求
        {
            mtx_list_user.lock();
            list_user.push_back(rel);
            mtx_list_user.unlock();
        }
        else   //客户端连接正常 有数据请求
        {
            int Length;
            char *Data_client = new char[packetsize];
            memset(Data_client, 0, packetsize);
            retVal = sock.Recv(sClient, (char*)&Length, 4);
            retVal = sock.Recv(sClient, Data_client, Length);
            if (retVal < 0)   //接收失败 客户端断开连接
            {
                sock.Close(sClient);
                mtx_map_user.lock();
                map_user.erase(rel.guid);
                mtx_map_user.unlock();
                delete[] Data_client;
            }
            else     //接收cmd 处理数据
            {
                Document DocRecv;
                DocRecv.Parse(Data_client);
                if (DocRecv.IsObject())
                {
                    if (DocRecv.HasMember("cmd"))
                    {
                        Value &vcmd = DocRecv["cmd"];
                        string cmd = vcmd.GetString();
                        unordered_map<string, int(*)(const char*, string)>::iterator it;
                        it = map_function.find(cmd);
                        if (it != map_function.end())
                        {
                            it->second(Data_client, rel.guid);
                        }
                        else  //不支持的cmd
                        {

                        }

                    }
                    else   //无cmd
                    {
                        cout << endl;
                    }
                    delete[] Data_client;
                }
                else   //错误的json
                {
                    cout << endl;
                }
                mtx_list_user.lock();
                list_user.push_back(rel);
                mtx_list_user.unlock();
            }
        }

    }
    return 0;
}

int ProxyServer::ServerReceiver()
{
    while (true)
    {
        mtx_list_user.lock();
        if (list_user.empty())
        {
            mtx_list_user.unlock();
            MSleep(1, "ms");
            continue;
        }
        Relation rel = list_user.front();
        list_user.pop_front();
        mtx_list_user.unlock();
        if (rel.sock == -1 || rel.sockid == -1)    //还没发送过connect指令的客户端连接
        {
            mtx_list_user.lock();
            list_user.push_back(rel);
            mtx_list_user.unlock();
            continue;
        }
        SOCKET sServer = rel.sock;
        fd_set fds_server;
        FD_ZERO(&fds_server);
        timeval tv = { 0,1 };
        FD_SET(sServer, &fds_server);
        if (select(sServer + 1, &fds_server, NULL, NULL, &tv) <= 0)  //无数据
        {
            mtx_list_user.lock();
            list_user.push_back(rel);
            mtx_list_user.unlock();
        }
        else
        {
            char *buf = new char[packetsize];
            memset(buf, 0, packetsize);
            int Length = sock.Recv(sServer, buf, packetsize);
            if (Length < 0)
            {

                sock.Close(sServer);
                //todo:通知客户端断开连接

            }
            else
            {
                Task task;
                Document DocSend;
                DocSend.SetObject();
                Value vdata;
                Value vguid;
                vguid.SetString(rel.guid.c_str(), DocSend.GetAllocator());
                vdata.SetString(buf, DocSend.GetAllocator());
                DocSend.AddMember("cmd", "receive", DocSend.GetAllocator());
                DocSend.AddMember("length", Length, DocSend.GetAllocator());
                DocSend.AddMember("sockid", rel.sockid, DocSend.GetAllocator());
                DocSend.AddMember("data", vdata, DocSend.GetAllocator());
                delete[] buf;

                string str;
                DocToStr(DocSend, str);
                task.Length = str.size();
                task.dst = GetSocket(rel.guid);
                task.data = new char[task.Length];
                memset(task.data, 0, task.Length);
                memcpy(task.data, str.c_str(), task.Length);
                mtx_list_task.lock();
                list_task.push_back(task);
                mtx_list_task.unlock();

                mtx_list_user.lock();
                list_user.push_back(rel);
                mtx_list_user.unlock();
            }
        }
    }

    return 0;
}

int ProxyServer::Verificator()
{
    cout << "Verification server running.." << endl;
    int retVal = 0;
    while (true)
    {
        SOCKET sClient = sock.Accept(sLocal);
        if (sClient == -1)
        {
            MSleep(1, "ms");
            continue;
        }
        else
        {
            int Length;
            char *buf = new char[packetsize];
            memset(buf, 0, packetsize);
            retVal = sock.Recv(sClient, (char*)&Length, sizeof(Length));
            retVal = sock.Recv(sClient, (char*)buf, Length);
            Document DocRecv;
            Document DocSend;
            DocSend.SetObject();
            DocRecv.Parse(buf);
            if (DocRecv.IsObject())
            {
                if (DocRecv.HasMember("username"))
                {
                    Value &vusername = DocRecv["username"];
                    if (strcmp(vusername.GetString(), username.c_str()) == 0)
                    {
                        if (DocRecv.HasMember("password"))
                        {
                            Value &vpassword = DocRecv["password"];
                            if (strcmp(vpassword.GetString(), password.c_str()) == 0)
                            {
                                char guid[64] = { 0 };
                                GenerateGuid(guid, 64);
                                Value vguid;
                                vguid.SetString(guid, 64, DocSend.GetAllocator());
                                DocSend.AddMember("status", "success", DocSend.GetAllocator());
                                DocSend.AddMember("guid", vguid, DocSend.GetAllocator());
                                SendDocument(DocSend, sClient);

                                mtx_map_user.lock();
                                map_user.insert(pair<string, SOCKET>(guid, sClient));
                                mtx_map_user.unlock();
                                mtx_list_user.lock();
                                string str_guid = guid;
                                Relation rel;
                                rel.guid = guid;
                                rel.sock = -1;
                                rel.sockid = -1;
                                list_user.push_back(rel);
                                mtx_list_user.unlock();

                                continue;
                            }
                        }
                    }
                }
            }
            else
            {
                DocSend.AddMember("status", "fail", DocSend.GetAllocator());
                SendDocument(DocSend, sClient);
                continue;
            }
        }
        MSleep(1, "ms");
    }
    return 0;
}

int ProxyServer::Connector()
{
    while (true)
    {
        mtx_list_connect.lock();
        if (list_connect.empty())
        {
            mtx_list_connect.unlock();
            MSleep(1, "ms");
            continue;
        }
        Connect_Request req;
        req = list_connect.front();
        list_connect.pop_front();
        mtx_list_connect.unlock();

        string IP = req.info.IP;
        int Port = req.info.Port;
        int64_t sockid = req.sockid;
        string guid = req.guid;
        SOCKET sClient = GetSocket(guid);
        SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        Document DocSend;
        DocSend.SetObject();
        int retVal;
        unsigned long mode = 1;
        ioctlsocket(sServer, FIONBIO, &mode);
        if (sock.Connect(sServer, IP.c_str(), Port) != 0)   //第一次连接失败 尝试重连
        {
            timeval tv = { 1,0 };  //连接超时1s
            fd_set Write;
            FD_ZERO(&Write);
            FD_SET(sServer, &Write);
            select(sServer + 1, NULL, &Write, NULL, &tv);
            if (!FD_ISSET(sServer, &Write))   //连接失败
            {
                DocSend.AddMember("cmd", "connect_rtn", DocSend.GetAllocator());
                DocSend.AddMember("status", "fail", DocSend.GetAllocator());
                DocSend.AddMember("sockid", sockid, DocSend.GetAllocator());
                sock.Close(sServer);
                string str;
                DocToStr(DocSend, str);
                Task task;
                task.dst = sClient;
                task.Length = str.size();
                task.data = new char[task.Length];
                memset(task.data, 0, task.Length);
                memcpy(task.data, str.c_str(), task.Length);
                mtx_list_task.lock();
                list_task.push_back(task);
                mtx_list_task.unlock();
            }
            else    //超时前连接成功
            {
                mode = 0; //阻塞socket
                ioctlsocket(sServer, FIONBIO, &mode);
                DocSend.AddMember("cmd", "connect_rtn", DocSend.GetAllocator());
                DocSend.AddMember("status", "success", DocSend.GetAllocator());
                DocSend.AddMember("sockid", sockid, DocSend.GetAllocator());

                string str;
                DocToStr(DocSend, str);
                Task task;
                task.dst = sClient;
                task.Length = str.size();
                task.data = new char[task.Length];
                memset(task.data, 0, task.Length);
                memcpy(task.data, str.c_str(), task.Length);
                mtx_list_task.lock();
                list_task.push_back(task);
                mtx_list_task.unlock();

                mtx_list_user.lock();
                list_user.push_back({ guid,sServer,sockid });
                mtx_list_user.unlock();

                int Token = tokenpool.alloc();
                Info_Full Info;
                Info.Token = Token;
                Info.sClient = sClient;
                Info.Sockid = sockid;
                Info.sServer = sServer;
                Info.Guid = guid;
                mtx_list_info_full.lock();
                list_Info_Full.push_back(Info);
                mtx_list_info_full.unlock();
            }
        }
        else    //第一次就连接成功
        {
            mode = 0; //阻塞socket
            ioctlsocket(sServer, FIONBIO, &mode);
            DocSend.AddMember("cmd", "connect_rtn", DocSend.GetAllocator());
            DocSend.AddMember("status", "success", DocSend.GetAllocator());
            DocSend.AddMember("sockid", sockid, DocSend.GetAllocator());

            string str;
            DocToStr(DocSend, str);
            Task task;
            task.dst = sClient;
            task.Length = str.size();
            task.data = new char[task.Length];
            memset(task.data, 0, task.Length);
            memcpy(task.data, str.c_str(), task.Length);
            mtx_list_task.lock();
            list_task.push_back(task);
            mtx_list_task.unlock();

            mtx_list_user.lock();
            list_user.push_back({ guid,sServer,sockid });
            mtx_list_user.unlock();

            int Token = tokenpool.alloc();
            Info_Full Info;
            Info.Token = Token;
            Info.sClient = sClient;
            Info.Sockid = sockid;
            Info.sServer = sServer;
            Info.Guid = guid;
            mtx_list_info_full.lock();
            list_Info_Full.push_back(Info);
            mtx_list_info_full.unlock();
        }

        MSleep(1, "ms");
    }
    return 0;
}

int ProxyServer::Forwarder()
{
    while (true)
    {
        mtx_list_task.lock();
        if (list_task.empty())
        {
            mtx_list_task.unlock();
            MSleep(1, "ms");
            continue;
        }
        Task task = list_task.front();
        list_task.pop_front();
        mtx_list_task.unlock();
        int retVal = sock.Send(task.dst, (char*)&task.Length, 4);
        retVal = sock.Send(task.dst, task.data, task.Length);
        delete[] task.data;
        MSleep(1, "ms");
    }
    return 0;
}
