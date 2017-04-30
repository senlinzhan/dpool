![Build status](https://https://travis-ci.org/senlinzhan/dpool.svg?branch=master)
# dpool 

使用 C++11 实现的动态线程池，主要特性：
- 使用简单，不易出错。 
- 支持线程复用，提升性能。
- 支持懒惰创建线程。
- 必要时自动回收空闲的线程。 

## 快速上手
```C++
#include "ThreadPool.hpp"
#include <iostream>

int compute(int a, int b)
{
    return a + b;
}

int main()
{
    // 设置最大线程数为 10
    dpool::ThreadPool pool(10);

    auto fut = pool.submit(compute, 100, 100);
    std::cout << "100 + 100 = " << fut.get() << std::endl;
    
    return 0;
}
```
