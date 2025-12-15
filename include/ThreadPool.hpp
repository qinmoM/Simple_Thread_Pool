/**
 * @file ThreadPool.hpp
 * @brief A Simple C++11 Thread Pool Implementation
 * 
 * @author qinmoM
 * @github https://github.com/qinmoM/Simple_Thread_Pool
 */

#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <thread>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <condition_variable>

/// @namespace qinmo
namespace qinmo
{

class ThreadPool
{
public:
    /// @brief Construction
    /// @param num_thread The number of threads
    explicit ThreadPool(std::size_t num_thread);

    /// @brief Destruction
    ~ThreadPool();

    /// @brief Copy and move constructors and assignment operators are deleted
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

public:
/*
            Adding tasks of the thread pool.
*/

    /// @brief add a task to be executed, and get its return value
    /// @tparam F ordinary function, lambda or functor
    /// @tparam ...Args any number of parameters
    /// @param f rvalue reference of the task function
    /// @param ...args parameters passed to task function
    /// @return std::future object that holds the return value of task function
    /// @throw std::runtime_error if the thread pool has been closed
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;


/*
            Closing of the thread pool.
*/

    /// @brief close the active thread pool
    /// @details complete all tasks
    void shutdown();

    /// @brief close the active thread pool
    /// @details abandon the tasks that have not yet been executed
    void shutdownNow();


/*
            Status of the thread pool.
*/

    /// @brief check if the thread pool has been closed
    /// @return return true if the thread pool has been closed, false otherwise
    /// @details this function will block. For debugging purpose only.
    bool isShutdown();

    /// @brief get the number of threads
    std::size_t getThreadCount() const;

    /// @brief get the number of tasks in the message queue
    /// @details this function will block. For debugging purpose only.
    std::size_t getQueueCount();

private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool shutdown_;

};

/*
                Implementation
*/

ThreadPool::ThreadPool(std::size_t num_thread)
    : shutdown_(false)
{
    for (int i = 0; i < num_thread; ++i)
    {
        threads_.emplace_back(
            [this]() -> void
            {
                while (true)
                {
                    std::function<void()> func;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cond_.wait(lock, [this]() -> bool { return shutdown_ || !queue_.empty(); });
                        if (shutdown_ && queue_.empty()) return;
                        func = std::move(queue_.front());
                        queue_.pop();
                    }
                    func();
                }
            }
        );
    }
}

ThreadPool::~ThreadPool()
{
    shutdown();
}

template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>
{
    using return_type = decltype(f(args...));
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (shutdown_) throw std::runtime_error("ThreadPool has been shutdown.");
        queue_.emplace([task]() -> void { (*task)(); });
    }
    cond_.notify_one();
    return res;
}

void ThreadPool::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return;
        shutdown_ = true;
    }
    cond_.notify_all();
    for (auto& thread : threads_)
    {
        if (thread.joinable())
            thread.join();
    }
}

void ThreadPool::shutdownNow()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (shutdown_) return;
        shutdown_ = true;
        // clear the message queue
        while (!queue_.empty()) queue_.pop();
    }
    cond_.notify_all();
    for (auto& thread : threads_)
    {
        if (thread.joinable())
            thread.join();
    }
}

bool ThreadPool::isShutdown()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return shutdown_;
}

std::size_t ThreadPool::getThreadCount() const
{
    return threads_.size();
}

std::size_t ThreadPool::getQueueCount()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

}