#include "TokenPool.h"



TokenPool::TokenPool()
{
    for (size = 0; size < 1000; ++size)
    {
        container.push_back(size);
    }
}
TokenPool::TokenPool(size_t size)
{
    for (size = 0; size < size; ++size)
    {
        container.push_back(size);
    }
}

int TokenPool::alloc()
{
    int Token;
    mtx_container.lock();
    if (container.empty())
    {
        mtx_container.unlock();
        return -2;   //tokenpoolÒÑÂú
    }
    else
    {
        Token = container.front();
        container.pop_front();
        mtx_container.unlock();
        return Token;
    }
}

inline int TokenPool::dealloc(int Token)
{
    mtx_container.lock();
    container.push_back(Token);
    mtx_container.unlock();
    return 0;
}
