#include <iostream>
#include "SocksProxy.h"

#ifdef _DEBUG
#pragma comment(lib,"Lib_Debug_x64.lib")
#else
#pragma comment(lib,"Lib_Release_x64.lib")
#endif

using namespace std;

SocksProxy proxy(9999);
void monitor()
{
#ifdef _WIN32
    HANDLE hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    system("cls");
#endif // _WIN32
    while (true)
    {
#ifdef _WIN32
        CONSOLE_CURSOR_INFO CursorInfo;
        GetConsoleCursorInfo(hCon, &CursorInfo);//获取控制台光标信息
        CursorInfo.bVisible = false; //隐藏控制台光标
        SetConsoleCursorInfo(hCon, &CursorInfo);//设置控制台光标状态
        SetConsoleCursorPosition(hCon, { 0,0 });
        cout << "任务队列:" << proxy.list_task.size() << endl;
        cout << "tcp连接数:" << proxy.list_relation.size() / 2 << endl;
        Sleep(10);
#endif // _WIN32

    }
}

int main()
{
    thread t1(monitor);
    t1.detach();
    int64_t a;
    proxy.Run();
    getchar();

}