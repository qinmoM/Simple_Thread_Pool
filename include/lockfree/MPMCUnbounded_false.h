#pragma once

#include "MPMCBounded.h"
#include <mutex>

namespace qinmo
{

template <typename T>
class MPMCUnbounded_false
{
public:
    MPMCUnbounded_false(std::size_t blockSize, std::size_t maxBlockNum = 1024)
        : blockSize_(blockSize)
        , maxBlockNum_(maxBlockNum)
        , size_(1)
    {
        queues_ = new std::unique_ptr<MPMCBounded<T>>[maxBlockNum];

        queues_[0] = qinmo::make_unique<MPMCBounded<T>>(blockSize);
    }

    ~MPMCUnbounded_false()
    {
        delete[] queues_;
    }

    MPMCUnbounded_false(const MPMCUnbounded_false&) = delete;
    MPMCUnbounded_false& operator=(const MPMCUnbounded_false&) = delete;

    template <typename... Args>
    bool emplace(Args&&... args)
    {
        while (true)
        {
            std::size_t size = size_.load(std::memory_order_acquire);
            for (std::size_t i = 0; i < size; ++i)
            {
                if (queues_[i]->emplace(std::forward<Args>(args)...))
                    return true;
            }

            if (size == maxBlockNum_)
                return false;

            {
                std::lock_guard<std::mutex> lock(mutex_);

                if (size_.load(std::memory_order_acquire) != size)
                    continue;

                queues_[size_] = qinmo::make_unique<MPMCBounded<T>>(blockSize_);
                size_.store(size + 1, std::memory_order_release);
            }
        }
    }

    bool push(const T& val)
    {
        return emplace(val);
    }

    bool pop(T& val)
    {
        for (std::size_t i = size_.load(std::memory_order_acquire) - 1; i != std::numeric_limits<std::size_t>::max(); --i)
        {
            if (queues_[i]->pop(val))
                return true;
        }

        return false;
    }

private:
    std::unique_ptr<MPMCBounded<T>>* queues_;
    std::atomic<std::size_t> size_;
    // std::atomic<std::size_t> index_;
    const std::size_t blockSize_;
    const std::size_t maxBlockNum_;

    std::mutex mutex_;
};
} // namespace qinmo
