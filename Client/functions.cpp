#include "functions.h"
extern ProxyClient client;
int Connect_Rtn(const char* buff)
{
    Document DocRecv;
    DocRecv.Parse(buff);

    if (DocRecv.HasMember("status") && DocRecv.HasMember("sockid"))
    {
        string status = DocRecv["status"].GetString();
        int64_t sockid = DocRecv["sockid"].GetInt64();

        if (strcmp(status.c_str(), "success") == 0)
        {
            client.mtx_list_request.lock();
            client.list_request.push_back(sockid);
            client.mtx_list_request.unlock();
            //client.mtx_map_Info.lock();
            //client.map_Info.insert(pair<SOCKET, Info>(sockid, { 0,0 }));
            //client.mtx_map_Info.unlock();

        }
        else
        {
            client.mtx_map_Info.lock();
            client.map_Info.erase(sockid);
            client.mtx_map_Info.unlock();
            client.sock.Close(sockid);
        }
    }
    else   //错误的返回包
    {

    }
    return 0;
}


int Pack_receive(const char* buff)
{
    Document DocRecv;
    DocRecv.Parse(buff);

    if (DocRecv.HasMember("sockid"))
    {
        int Length = DocRecv["length"].GetInt();
        int64_t sockid = DocRecv["sockid"].GetInt64();
        string szData = DocRecv["data"].GetString();
        char* data = new char[client.packetsize];
        memset(data, 0, client.packetsize);
        memcpy(data, szData.c_str(), Length);
        client.sock.Send(sockid, data, Length);
    }
    else   //错误的返回包
    {

    }
    return 0;
}



int InitFunc()
{
    int(*pConnect_Rtn)(const char*) = &Connect_Rtn;
    int(*pPack_receive)(const char*) = &Pack_receive;

    client.map_function.insert(pair<string, int(*)(const char*)>("connect_rtn", pConnect_Rtn));
    client.map_function.insert(pair<string, int(*)(const char*)>("receive", pPack_receive));
    return 0;
}