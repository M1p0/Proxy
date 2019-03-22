#undef  WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <MSocket.h>
#include <iostream>
#include "Utils.h"
#include "ProxyServer.h"
#include "TokenPool.h"
#ifdef _DEBUG
#pragma comment(lib,"Lib_Debug_x64.lib")
#else
#pragma comment(lib,"Lib_Release_x64.lib")
#endif

using namespace std;
ProxyServer server;
int main()
{
    server.Run();
}
