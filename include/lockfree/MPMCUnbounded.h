#pragma once

#include "MPMCBounded.h"
#include <mutex>
#include <vector>


template <typename T, template <typename> class MPMCBoundedLockFree>
struct Segment
{
    std::unique_ptr<MPMCBoundedLockFree<T>> segment_ = nullptr;
    std::atomic<std::size_t> pushOwnership_{ 0 };
    std::atomic<std::size_t> pushCount_{ 0 };
    std::atomic<std::size_t> popOwnership_{ 0 };
    std::atomic<std::size_t> popCount_{ 0 };
    Segment* next_ = nullptr;
};


template <typename T, template <typename> class MPMCBoundedLockFree>
class SegmentQueue
{
public:
    SegmentQueue(std::size_t blickSize)
        : blockSize_(blickSize)
    { }

    void push(Segment<T, MPMCBoundedLockFree>* segment)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(segment);
        }
    }

    Segment<T, MPMCBoundedLockFree>* pop()
    {
        Segment<T, MPMCBoundedLockFree>* ret = nullptr;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty())
            {
                Segment<T, MPMCBoundedLockFree>* seg = new Segment<T, MPMCBoundedLockFree>();
                seg->segment_ = qinmo::make_unique<MPMCBoundedLockFree<T>>(blockSize_);
                queue_.push_back(seg);
            }

            ret = queue_.back();
            ret->next_ = nullptr;
            ret->popCount_.store(0, std::memory_order_release);
            ret->popOwnership_.store(0, std::memory_order_release);
            ret->pushCount_.store(0, std::memory_order_release);
            ret->pushOwnership_.store(0, std::memory_order_release);
            queue_.pop_back();
        }

        return ret;
    }

private:
    std::mutex mutex_;
    std::vector<Segment<T, MPMCBoundedLockFree>*> queue_;
    const std::size_t blockSize_;

};



template <typename T, template <typename> class MPMCBoundedLockFree = MPMCBounded>
class MPMCUnbounded
{
public:
    MPMCUnbounded(std::size_t blockSize)
        : blockSize_(blockSize)
        , queues_(blockSize)
    {
        head_.store(queues_.pop(), std::memory_order_release);
        tail_.store(head_, std::memory_order_release);
    }

    ~MPMCUnbounded()
    {
        Segment<T, MPMCBoundedLockFree>* curr = head_.load(std::memory_order_acquire);

        while (nullptr != curr)
        {
            queues_.push(curr);
            curr = curr->next_;
        }
    }

    template <typename... Args>
    bool emplace(Args&&... args)
    {
        Segment<T, MPMCBoundedLockFree>* seg = tail_.load(std::memory_order_acquire);
        std::size_t ownership = seg->pushOwnership_.load(std::memory_order_acquire);
        while (true)
        {
            if (ownership == blockSize_)
            {
                seg = tail_.load(std::memory_order_acquire);
                ownership = seg->pushOwnership_.load(std::memory_order_acquire);
                continue;
            }

            if (seg->pushOwnership_.compare_exchange_strong(ownership, ownership + 1))
            {
                if (!seg->segment_->emplace(std::forward<Args>(args)...))
                {
                    // what happen?
                    std::exit(-1);
                }

                std::size_t count = seg->pushCount_.fetch_add(1) + 1;
                if (count == blockSize_)
                {
                    std::lock_guard<std::mutex> lock(mutex_);

                    seg->next_ = queues_.pop();
                    tail_.store(seg->next_, std::memory_order_release);
                }
                return true;
            }
        }
    }

    bool push(const T& val)
    {
        return emplace(val);
    }

    bool pop(T& val)
    {
        Segment<T, MPMCBoundedLockFree>* seg = head_.load(std::memory_order_acquire);
        std::size_t ownership = seg->popOwnership_.load(std::memory_order_acquire);
        while (true)
        {
            if (blockSize_ == ownership)
            {
                seg = head_.load(std::memory_order_acquire);
                ownership = seg->popOwnership_.load(std::memory_order_acquire);
                continue;
            }

            if (seg->popOwnership_.compare_exchange_strong(ownership, ownership + 1))
            {
                if (!seg->segment_->pop(val))
                {
                    seg->popOwnership_.fetch_sub(1);
                    return false;
                }

                std::size_t count = seg->popCount_.fetch_add(1) + 1;
                if (count == blockSize_)
                {
                    std::lock_guard<std::mutex> lock(mutex_);

                    head_.store(seg->next_, std::memory_order_release);
                    queues_.push(seg);
                }
                return true;
            }
        }
    }

private:
    SegmentQueue<T, MPMCBoundedLockFree> queues_;
    std::atomic<Segment<T, MPMCBoundedLockFree>*> head_;
    std::atomic<Segment<T, MPMCBoundedLockFree>*> tail_;

    std::mutex mutex_;
    const std::size_t blockSize_;

};