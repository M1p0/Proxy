#include <iostream>
#include <thread>
#include <string>
#include <iostream>
#include <MSocket.h>
#include "SocksProxy.h"
#include <Public.h>
#include <MemPool.h>

#ifdef _DEBUG
#pragma comment(lib,"Lib.lib")
#else
#pragma comment(lib,"Lib_Release.lib")
#endif

using namespace std;
int main()
{
    SocksProxy proxy(9999);
    proxy.Run();
}

