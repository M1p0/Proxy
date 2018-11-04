#pragma once
#include <MSocket.h>
#include <iostream>
#include "MString.h"
class SocksProxy
{
public:
    SocksProxy(int iPort);
    int Init(int iPort);
    int Run();
    virtual ~SocksProxy() {};

private:
    int Mediator(SOCKET sClient);
    int Listener(SOCKET sServer);
    int Confirm(SOCKET sClient);
    MSocket sock;
    SOCKET sServer;
};


