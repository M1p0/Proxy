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

SocksProxy proxy(9999);
void show()
{
    while (true)
    {
        cout << proxy.list_relation.size() << endl;
    }
}

int main()
{

    thread t1(show);
    t1.detach();
    proxy.Run();

}

