#include <iostream>
#include <thread>
#include <string>
#include <iostream>
#include <MSocket.h>
#include "SocksProxy.h"
#pragma comment(lib,"Lib.lib")


using namespace std;




int main()
{
    SocksProxy proxy(9999);
    proxy.Run();
}


