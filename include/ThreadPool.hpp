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

namespace qinmo
{

class ThreadPool
{
public:
    explicit ThreadPool(std::size_t num_thread);
    ~ThreadPool();
public:
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<decltype(f(args...))>;

    void shutdown();
    void shutdownNow();

    bool isShutdown();
    std::size_t getThreadCount() const;
    std::size_t getQueueCount();
private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool shutdown_;
};

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