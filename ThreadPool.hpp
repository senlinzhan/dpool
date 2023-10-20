#ifndef dpool_THREADPOOL_H
#define dpool_THREADPOOL_H

#ifdef _WIN32
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#endif //win32
/*
* @file dpool线程池
* @brief 用c++11写的跨平台线程池，支持c++14 c++17语法标准
*       - 使用简单，不易出错。 
        - 支持线程复用，提升性能。
        - 支持懒惰创建线程。
        - 必要时自动回收空闲的线程。 
* @author senlinzhan  | fgfxf
* @date 2017  | 2023改进
* @note 用chartGPT辅助下增加了对c++17 20版本的支持，增加了一些get set函数
* @note 线程池会自动回收空闲线程，无需从外部主动释放，请放心上船
* @warning 作为合格的开发者，应当保证线程池单例化，因此函数和声明没有分离，
*        如果想要多实例的线程池，请把本hpp文件中的“非模板类成员函数”分离出来即可。
* @warning set方法没有做健壮性检测，作为合格的开发者，相信您不会犯这样的错误。
*           
* @see https://github.com/senlinzhan/dpool
*/

//#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

void myassert(bool test) {
    if (!test) {
        std::runtime_error("error at dpool ");
    }
}

#define assert myassert

namespace dpool {  

class ThreadPool
{
public:
    using MutexGuard = std::lock_guard<std::mutex>;
    using UniqueLock = std::unique_lock<std::mutex>;
    using Thread     = std::thread;
    using ThreadID   = std::thread::id;
    using Task       = std::function<void()>;
    /*
    * @brief dpool初始化，自动获取数量
    * @param 最大线程数自动获取硬件数量
    */
    ThreadPool()
        : ThreadPool(Thread::hardware_concurrency())//自动获取cpu硬件线程数
    {        
    }
    /*
    * @brief dpool初始化，指定最大线程数
    * @param 最大线程数
    */
    explicit ThreadPool(size_t maxThreads)
        : quit_(false),
          currentThreads_(0), 
          idleThreads_(0),
          maxThreads_(maxThreads),
          WAIT_MINUTES(10),
          minThreads_(2)
    {        
    }

    // disable the copy operations
    /*
    * @brief 禁止拷贝
    */
    ThreadPool(const ThreadPool &) = delete;
    /*
    * @brief 禁止拷贝
    */
    ThreadPool &operator=(const ThreadPool &) = delete;
    
    /*
    * @brief 析构线程池
    */
    ~ThreadPool()
    {
        {
            MutexGuard guard(mutex_);
            quit_ = true;
        }
        cv_.notify_all();//唤醒所有线程
        
        for (auto &elem: threads_)
        {
            assert(elem.second.joinable());
            elem.second.join();//逐个线程地等待退出
        }
    }
    /*
    * @brief 初始化线程池（非必要）
    *  当创建好线程池对象后，当前工作线程数量为0，调用此函数增加工作线程
    *          非必要操作，适合强迫症程序员
    */
    void Initialize() {
        MutexGuard guard(mutex_);
        if (quit_)
            return;
        while (currentThreads_ < minThreads_)
        {
            Thread t(&ThreadPool::worker, this);
            assert(threads_.find(t.get_id()) == threads_.end());
            threads_[t.get_id()] = std::move(t);//map线程id---线程 对应
            ++currentThreads_;
        }
    }


    /*
    * @brief dpool提交任务
    * @param func函数
    * @param ...变长参数，func的函数列表
    * @return 利用标准库函数std::future异步获取函数的返回值
    */
    template<typename Func, typename... Ts>
    auto submit(Func &&func, Ts &&... params) 
      //  -> std::future<typename std::result_of<Func(Ts...)>::type>        
    {
        //函数参数转发---完美转发
        auto execute = std::bind(std::forward<Func>(func), std::forward<Ts>(params)...);
#if (defined(_MSVC_LANG ) && _MSVC_LANG  > 201402L) ||  __cplusplus> 201402L
        //当使用msvc编译器，则_MSVC_LANG。否则gcc/g++ __cplusplus
        //msvc中__cplusplus始终是98年，f**k
        using ReturnType = std::invoke_result_t<Func, Ts...>; //利用编译期运算获取函数返回类型
#else
        using ReturnType = typename std::result_of<Func(Ts...)>::type;
#endif
       
        using PackagedTask = std::packaged_task<ReturnType()>;
        //移动execute变成task
        auto task = std::make_shared<PackagedTask>(std::move(execute)); 
        auto result = task->get_future();
        
        MutexGuard guard(mutex_);
        assert(!quit_);
        //tasks任务列表添加一个task
        tasks_.emplace([task]()
                       {
                           (*task)();
                       });
        //当空闲线程>0,就执行一个
        if (idleThreads_ > 0)
        {
            cv_.notify_one(); //唤醒一个线程
        }
        //创建新的线程
        else if (currentThreads_ < maxThreads_)
        {
            Thread t(&ThreadPool::worker, this);            
            assert(threads_.find(t.get_id()) == threads_.end());
            threads_[t.get_id()] = std::move(t);//map线程id---线程 对应
            ++currentThreads_;
        }        

        return result;
    }

    /*
    * @brief 返回当前活动线程的数量
    * @return size_t线程数量
    */
    size_t getCurrenThreadsNum() const
    {

        return currentThreads_;
    }
    /*
    * @brief 返回当前线程池最大线程数量
    * @return size_t线程数量
    */
    size_t getMaxThreadsNum() {
        return maxThreads_;
    }
    /*
    * @brief 设置最大线程数量
    * @param size_t线程数量
    */
    void setMaxThreadsNum(size_t szNewMaxThreadCount) {
        maxThreads_ = szNewMaxThreadCount;
    }

    /*
    * @brief 返回当前线程池最小线程数量
    * @return size_t线程数量
    */
    size_t getMinThreadsNum() {
        return minThreads_;
    }
    /*
    * @brief 设置最小线程数量
    * @param size_t线程数量
    */
    void setMinThreadsNum(size_t szNewMinThreadCount) {
        minThreads_ = szNewMinThreadCount;
    }


    /*
    * @brief 获取线程空闲等待销毁时长
    * @param size_t单位：分钟
    */
    size_t getWaitMinutes() {
        return WAIT_MINUTES;
    }
    /*
    * @brief 设置线程空闲销毁时长，单位:分钟
    */
    void setWaitMinutes(size_t WaitMinutes) {
        UniqueLock uniqueLock(mutex_);
        WAIT_MINUTES = WaitMinutes;
    }

private:
    /*
    * @brief 工作线程，submit函数提交任务之后，如果没有线程就会创建线程。
    * 子线程执行这里
    */
    void worker()
    {
        while (!quit_)
        {
            Task task;
                {//代码块 加锁
                    UniqueLock uniqueLock(mutex_);
                    ++idleThreads_;            
                
                    //工作线程休眠：当超时WAIT_MINUTES或者quit__或者任务非空时唤醒。
                    bool hasTimedout = !cv_.wait_for(uniqueLock,
                                                     std::chrono::minutes(WAIT_MINUTES),
                                                     [this]()
                                                     {
                                                         return quit_ || !tasks_.empty();
                                                     });                
                    --idleThreads_;
                    //如果任务列表空了
                    if (tasks_.empty())
                    {
                        if (quit_)//如果退出 
                        {
                            --currentThreads_;//线程会由析构函数回收，不做处理
                            return;
                        }
                        //超时销毁线程
                        if (hasTimedout)
                        {
                            DestroyFinishedThreads();//清理其他线程
                            if (currentThreads_ > minThreads_) {
                                //如果线程比较多
                                --currentThreads_;
                                finishedThreadIDs_.emplace(std::this_thread::get_id());//当前线程自己加入到待清理队列                      
                                return;
                            }
                            continue;//继续循环
                        }
                    }

                    //任务列表不为空时，执行队列函数
                    task = std::move(tasks_.front());
                    tasks_.pop();
                 //代码解锁
                }
            task();
        }
    }
    /*
    * @brief 清理线程
    * 清理所有加入到finishedThreadIDs_队列的线程
    */
    void DestroyFinishedThreads()
    {
        //循环将所有finishedThreadIDs_队列中的线程都释放掉
        //finishedThreadIDs_是完成任务的线程id队列
        while (!finishedThreadIDs_.empty())
        {
            auto id = std::move(finishedThreadIDs_.front());
            finishedThreadIDs_.pop();            
            auto iter = threads_.find(id); 
            assert(iter != threads_.end());
            assert(iter->second.joinable());
            iter->second.join();
            threads_.erase(iter);
        }                                
    }

    size_t                                 WAIT_MINUTES ;    //线程等待销毁，若任务列表空，等待多少时长线程池被优化
    bool                                  quit_;                //退出标志
    size_t                                currentThreads_;     //当前线程数量
    size_t                                idleThreads_;       //空闲线程数量
    size_t                                maxThreads_;         //最大线程数量
    size_t                                 minThreads_;         //最小线程数量，保证突发性
    mutable std::mutex                    mutex_;   //互斥体
    std::condition_variable               cv_;     //条件变量，用于线程唤醒
    std::queue<Task>                      tasks_; //任务队列
    std::queue<ThreadID>                  finishedThreadIDs_;//完成的线程ID
    std::unordered_map<ThreadID, Thread>  threads_;  //map   线程id -- 线程
};
    
}  // namespace dpool

#endif /* THREADPOOL_H */
