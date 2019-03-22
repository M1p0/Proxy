#pragma once
#include <list>
#include <mutex>
class TokenPool
{
public:
    TokenPool();
    TokenPool(size_t size);
    int alloc();
    int dealloc(int Token);
private:
    int size;
    std::mutex mtx_container;
    std::list<int> container;
};
