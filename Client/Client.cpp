#include <iostream>
#include <thread>
#include <string>
#include <iostream>
#include <MSocket.h>
#include "SocksProxy.h"
#include <Public.h>
#include <MemPool.h>
#pragma comment(lib,"Lib.lib")


using namespace std;



int main()
{   
    SocksProxy proxy(9999);

    proxy.Run();
    while (true)
    {
        cout << proxy.list_relation.size() << endl;
        MSleep(1, "ms");
    }
}


