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
    MPMCUnbounded(std::size_t blockSize)
        : blockSize_(blockSize)
        , index_(0)
    {
        queues_.emplace_back<MPMCBounded<T>>(
            qinmo::make_unique(blockSize)
        );
    }

    MPMCUnbounded(const MPMCUnbounded&) = delete;
    MPMCUnbounded& operator=(const MPMCUnbounded&) = delete;

    template <typename... Args>
    bool emplace(Args&&... args)
    {
        std::size_t index = index_.load(std::memory_order_acquire);

        while (true)
        {
            if (queues_[index]->emplace(std::forward<Args>(args)...))
            {
                updateIndex(index);
                return true;
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                std::size_t currIndex = index_.load(std::memory_order_acquire);

                if (index != currIndex)
                {
                    index = currIndex;
                    continue;
                }

                if (queues_[index]->emplace(std::forward<Args>(args)...))
                {
                    updateIndex(index);
                    return true;
                }
                else
                {
                    toNext(index);
                }
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
            if (queues_[index]->pop(val))
                return true;

            if (0 == index)
                return false;

            if (index_.compare_exchange_strong(index, index - 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed))
            {
                --index;
            }
        }
    }

private:
    void updateIndex(std::size_t index)
    {
        std::size_t curr = index_.load(std::memory_order_relaxed);
        while (curr < index &&
            !index_.compare_exchange_weak(curr, index,
                std::memory_order_release,
                std::memory_order_relaxed)
        ) { }
    }

    void toNext(std::size_t index)
    {
        if (index + 1 == queues_.size())
        {
            queues_.emplace_back<MPMCBounded<T>>(
                qinmo::make_unique(blockSize)
            );
            index_.compare_exchange_strong(index, index + 1);
        }
    }

private:
    std::vector<std::unique_ptr<MPMCBounded<T>>> queues_;
    std::atomic<std::size_t> index_;
    const std::size_t blockSize_;

    std::mutex mutex_;
};
} // namespace qinmo
