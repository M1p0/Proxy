#include <MSocket.h>
#include "functions.h"
#include "MJson.h"
#include "ProxyServer.h"
extern ProxyServer server;



int Connect(const char* buff, string guid)
{
    Document DocRecv;
    Document DocSend;
    DocSend.SetObject();
    DocRecv.Parse(buff);
    if (DocRecv.IsObject())
    {
        string IP;
        int Port;
        int64_t sockid;
        if (server.isVerificated(guid))
        {
            if (DocRecv.HasMember("ip") && DocRecv.HasMember("port") && DocRecv.HasMember("sockid"))
            {
                Value &vip = DocRecv["ip"];
                Value &vport = DocRecv["port"];
                Value &vsockid = DocRecv["sockid"];
                IP = vip.GetString();
                Port = vport.GetInt();
                sockid = vsockid.GetInt64();
                Connect_Request req;
                req.info.IP = IP;
                req.info.Port = Port;
                req.sockid = sockid;
                req.guid = guid;
                server.mtx_list_connect.lock();
                server.list_connect.push_back(req);
                server.mtx_list_connect.unlock();
            }
            else  //数据包不完整 无视
            {
            }
        }
        else  //非法用户 伪造guid
        {
            return -1;
        }
    }
    else
    {
        return -1;
    }

    return 0;
}


int Forward(const char* buff, string guid)
{
    Document DocRecv;
    Document DocSend;
    DocSend.SetObject();
    DocRecv.Parse(buff);
    if (DocRecv.HasMember("guid"))
    {
        string guid = DocRecv["guid"].GetString();
        if (DocRecv.HasMember("data") && DocRecv.HasMember("length") && DocRecv.HasMember("sockid"))
        {
            int Length = DocRecv["length"].GetInt();
            int64_t sockid = DocRecv["sockid"].GetInt64();
            char* data = new char[Length];
            memset(data, 0, Length);
            memcpy(data, DocRecv["data"].GetString(), Length);
            Task task;
            task.data = data;
            task.Length = Length;
            task.dst = -1;
            while (true)
            {
                server.mtx_list_user.lock();
                for (auto it = server.list_user.begin(); it != server.list_user.end(); ++it)
                {
                    if (it->sockid == sockid && it->guid == guid)
                    {
                        task.dst = it->sock;
                        break;
                    }
                }
                server.mtx_list_user.unlock();
                if (task.dst!=-1)
                {
                    break;
                }
            }
            server.mtx_list_task.lock();
            server.list_task.push_back(task);
            server.mtx_list_task.unlock();
        }
        else
        {
            //
        }
    }
    else
    {
        //无guid
    }
    return 0;
}



int InitFunc()
{
    int(*pConnect)(const char*, string) = &Connect;
    int(*pForward)(const char*, string) = &Forward;

    server.map_function.insert(pair<string, int(*)(const char*, string)>("connect", pConnect));
    server.map_function.insert(pair<string, int(*)(const char*, string)>("forward", pForward));
    return 0;
}
