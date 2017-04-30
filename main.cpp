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

    // executing task for 2 second
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    {
        std::lock_guard<std::mutex> guard(coutMtx);
        std::cout << "task-" << taskId << " end!" << std::endl;        
    }
}


void monitor(const ThreadPool &pool, int seconds)
{
    for (int i = 1; i < seconds * 10; ++i)
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
    // max threads number is 100
    ThreadPool pool(100); 

    // monitoring threads number for 13 seconds    
    pool.submit(monitor, std::ref(pool), 13);
    
    // submit 100 tasks
    for (int taskId = 0; taskId < 100; ++taskId)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        pool.submit(task, taskId);
    }
    
    return 0;
}
