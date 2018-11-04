#include "SocksProxy.h"
#include <thread>
#include <string>
#include <Public.h>
#include "MString.h"
using namespace std;

SocksProxy::SocksProxy(int iPort)
{
    Init(iPort);
}

int SocksProxy::Confirm(SOCKET sClient)
{
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
        return 1;
    }
    return 0;
}

int SocksProxy::Init(int iPort)
{
    sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock.Bind(sServer, iPort, AF_INET)<0)
    {
        cout << "Bind Failed" << endl;
    }
    return 0;
}

int SocksProxy::Run()
{
    thread t_Listener(&SocksProxy::Listener, this, sServer);
    t_Listener.join();
    return 0;
}

int SocksProxy::Mediator(SOCKET sClient)
{
    char temp[1024];
    memset(temp, 0, 1024);
    int retVal = 0;
    if (Confirm(sClient))
    {
        string IP;
        int Port = 0;
                                           //     ipv4        ip          �˿�
        unsigned char Request_Info[4];     // 05 01 00 01  xx xx xx xx   xx xx
        unsigned char IP_Info[6];    //IP 4�ֽ�  Port 2�ֽ�
        memset(Request_Info, 0, 4);
        memset(IP_Info, 0, 6);
        retVal = sock.Recv(sClient, (char*)Request_Info, 4);
        if (Request_Info[3] == 0x01)
        {
            sock.Recv(sClient, (char*)IP_Info, 6);   //��ȡ�����ip��ַ�Ͷ˿�
            char sreply[10] = { 0x05,0x00,0x00,0x01 };
            for (int i = 4; i < 10; i++)
            {
                sreply[i] = IP_Info[i-4];
            }
            sock.Send(sClient, sreply, 10);  //05 00 00  IP Port �������

            //֮����socks5�����޹� ��ʼת������

            for (int i = 0; i < 4; i++)
            {
                char temp[4];   //���Ϊ0xFF 255 3λ��  ��Ҫ����'\0'
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


            memcpy(((char*)&Port), IP_Info + 5, 1);  //��С��ת��
            memcpy(((char*)&Port) + 1, IP_Info +4, 1);

        }
                                            //   ��������     ��������     ����        �˿�
                                            // 05 01 00 03      xx    xxxxxxxxxxx   xx xx
        else if(Request_Info[3] == 0x03)   
        {
            unsigned char Length=0;
            sock.Recv(sClient, (char*)&Length, 1);
            char Domain[256];
            memset(Domain, 0, 256);
            sock.Recv(sClient, Domain, Length);     //url
            char Temp[256];
            memset(Temp, 0, 256);
            sock.GetHostByName(Domain,Temp, 256);   //url->IP
            IP = Temp;
            memset(Temp, 0, 256);
            sock.Recv(sClient,Temp, 2);   //Port
            memcpy(((char*)&Port), Temp + 1, 1);  //��С��ת��
            memcpy(((char*)&Port) + 1, Temp, 1);  


            char sreply[4] = { 0x05,0x00,0x00,0x03 };
            sock.Send(sClient, sreply, 4);  //05 00 00 03 Length_domain domian Port �������
            sock.Send(sClient, (char*)&Length, 1);
            sock.Send(sClient, Domain, Length);
            sock.Send(sClient, Temp, 2);
        }
        else  //��֧�ֵ�ģʽ(IPv6)
        {
            sock.Close(sClient);
            return -1;
        }


        //����Ŀ������
        SOCKET sServer = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   //tcp
        if (sock.Connect(sServer, IP.c_str(), Port) != 0)             //����Ŀ�������
        {
            return 0;
        }


        fd_set fds_client;
        FD_ZERO(&fds_client);

        fd_set fds_server;
        FD_ZERO(&fds_server);
        timeval tv = { 0,10 };

        //��ʼ��Ϣת��
        while (true)
        {
            while (true)
            {
                timeval tv_temp = tv;
                if (!FD_ISSET(sClient, &fds_client))
                {
                    FD_SET(sClient, &fds_client);
                    MSleep(1, "ms");
                }
                if (select(sClient+1, &fds_client, NULL, NULL, &tv_temp) <= 0)
                {
                    break;
                }
                char Data_Client[1024];   // request
                memset(Data_Client, 0, 1024);
                retVal = sock.Recv(sClient, Data_Client, 1024);
                if (retVal < 0)
                {
                    sock.Close(sClient);
                    sock.Close(sServer);
                    return 0;
                }
                if (sock.Send(sServer, Data_Client, retVal) < 0)
                {
                    cout << "############Send To Server Failed############" << endl;
                }

                MSleep(1, "ms");
            }

            while (true)
            {
                timeval tv_temp = tv;

                if (!FD_ISSET(sServer, &fds_server))
                {
                    FD_SET(sServer, &fds_server);
                    MSleep(1, "ms");
                }
                if (select(sServer + 1, &fds_server, NULL, NULL, &tv_temp) <= 0)
                {
                    break;
                }

                char Data_Server[4096];   // response
                memset(Data_Server, 0, 4096);
                retVal = sock.Recv(sServer, Data_Server, 4096);
                if (retVal < 0)
                {
                    sock.Close(sClient);
                    sock.Close(sServer);
                    return 0;
                }

                if (sock.Send(sClient, Data_Server, retVal) < 0)
                {
                    cout << "############Send To Browser Failed############" << endl;
                }
                MSleep(1, "ms");
            }
        }


    }
    else
    {
        sock.Close(sClient);
        return -1;   //����socks5��������
    }
    return 0;
}

int SocksProxy::Listener(SOCKET sServer)
{
    cout << "Listener running..." << endl;
    sock.Listen(sServer, 5);
    while (true)
    {
        SOCKET sClient = sock.Accept(sServer);
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
            thread t_Mediator(&SocksProxy::Mediator, this, sClient);
            t_Mediator.detach();
        }
        MSleep(1,"ms");
    }
}
