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
        , index_(0)
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
                    std::size_t size = size_.load(std::memory_order_acquire);
                    if (size == maxBlockNum_)
                        return false;

                    toNext(index, size);
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

    void toNext(std::size_t index, std::size_t size)
    {
        if (index + 1 == size)
        {
            queues_[size_] = qinmo::make_unique<MPMCBounded<T>>(blockSize_);
            size_.store(size + 1, std::memory_order_release);
            index_.compare_exchange_strong(index, index + 1);
        }
    }

private:
    std::unique_ptr<MPMCBounded<T>>* queues_;
    std::atomic<std::size_t> size_;
    std::atomic<std::size_t> index_;
    const std::size_t blockSize_;
    const std::size_t maxBlockNum_;

    std::mutex mutex_;
};
} // namespace qinmo
