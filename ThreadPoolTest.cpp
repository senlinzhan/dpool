#include "ThreadPool.hpp"

#include <iostream>
#include <chrono>
#include <mutex>

using namespace dpool;

std::mutex coutMtx;  // protect std::cout

void task(int taskId)
{    
    {
        std::lock_guard<std::mutex> guard(coutMtx);
        std::cout << "task-" << taskId << " begin!" << std::endl;                    
    }

    // executing task for 1 second
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    {
        std::lock_guard<std::mutex> guard(coutMtx);
        std::cout << "task-" << taskId << " end!" << std::endl;        
    }
}

// monitoring threads number for 20 seconds
void monitor(const ThreadPool &pool)
{
    for (int i = 1; i < 200; ++i)
    {
        {
            std::lock_guard<std::mutex> guard(coutMtx);
            std::cout << "thread num: " << pool.threadsNum() << std::endl;                    
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));            
    }
}

int main(int argc, char *argv[])
{
    ThreadPool pool(100);  // 100 threads

    pool.submit(monitor, std::ref(pool));
    
    // submit 100 tasks
    for (int taskId = 0; taskId < 100; ++taskId)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pool.submit(task, taskId);
    }
    
    return 0;
}
