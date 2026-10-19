#pragma once

#include "MPMCBounded.h"
#include <mutex>
#include <vector>


template <typename T, template <typename> class MPMCBoundedLockFree>
struct Segment
{
    std::unique_ptr<MPMCBoundedLockFree<T>> segment_ = nullptr;
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
        : queues_(blockSize)
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
        while (true)
        {
            if (seg->segment_->emplace(std::forward<Args>(args)...))
                return true;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                Segment<T, MPMCBoundedLockFree>* currSeg = tail_.load(std::memory_order_acquire);

                if (currSeg != seg)
                {
                    seg = currSeg;
                    continue;
                }

                seg->next_ = queues_.pop();
                tail_.store(seg->next_, std::memory_order_release);
                seg = tail_.load(std::memory_order_acquire);
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
        while (true)
        {
            if (seg->segment_->pop(val))
                return true;

            {
                std::lock_guard<std::mutex> lock(mutex_);
                Segment<T, MPMCBoundedLockFree>* currSeg = head_.load(std::memory_order_acquire);

                if (currSeg != seg)
                {
                    seg = currSeg;
                    continue;
                }

                if (tail_.load(std::memory_order_acquire) == seg)
                    return false;

                head_.store(seg->next_, std::memory_order_release);
                queues_.push(seg);
                seg = head_.load(std::memory_order_acquire);
            }
        }
    }

private:
    SegmentQueue<T, MPMCBoundedLockFree> queues_;
    std::atomic<Segment<T, MPMCBoundedLockFree>*> head_;
    std::atomic<Segment<T, MPMCBoundedLockFree>*> tail_;

    std::mutex mutex_;

};