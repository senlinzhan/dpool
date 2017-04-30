# dpool
使用 C++11 实现的动态线程池

**快速上手**
```C++
#include "ThreadPool.hpp"
#include <iostream>

int compute(int a, int b)
{
    return a + b;
}

int main(int argc, char *argv[])
{
    // 设置最大线程数为 10
    dpool::ThreadPool pool(10);

    auto fut = pool.submit(compute, 100, 100);
    std::cout << "100 + 100 = " << fut.get() << std::endl;
    
    return 0;
}
```
