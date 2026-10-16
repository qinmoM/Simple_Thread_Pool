#pragma once

#include "MPMCBounded.h"
#include <vector>
#include <mutex>

namespace qinmo
{

template <typename T>
class MPMCUnbounded
{
public:
    MPMCUnbounded(std::size_t capacity)
        , capacity_(capacity)
        , index_(0)
        , size_(1)
    {
        queues_.emplace(capacity);
    }

    MPMCUnbounded(const MPMCUnbounded&) = delete;
    MPMCUnbounded& operator=(const MPMCUnbounded&) = delete;

    template <typename... Args>
    bool emplace(Args&&... args)
    {
        std::size_t index = index_.load(std::memory_order_acquire);

        while (true)
        {
            if (queues_[index].emplace(std::forward<Args>(args)...))
                return true;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                std::size_t currIndex = index_.load(std::memory_order_acquire);

                if (index != currIndex)
                {
                    index = currIndex;
                    continue;
                }

                if (queues_[index].emplace(std::forward<Args>(args)...))
                    return true;
                else
                    toNext(index);
            }
        }
    }

    bool push(const T& val)
    {
        return emplace(val);
    }

    bool pop(T& val)
    {
        std::size_t index = index_.load(std::memory_order_acquire);

        while (true)
        {
            if (queues_[index].pop(val))
                return true;

            if (0 == index)
                return false;

            index_.compare_exchange_strong(index, index - 1);
        }
    }

private:
    void toNext(std::size_t index)
    {
        if (index + 1 == queues_.size())
        {
            queues_.emplace(capacity_);
            index_.compare_exchange_strong(index, index + 1);
        }
    }

private:
    std::vector<std::unique_ptr<MPMCBounded<T>>> queues_;
    std::atomic<std::size_t> index_;
    const std::size_t capacity_;

    std::mutex mutex_;
};
} // namespace qinmo
