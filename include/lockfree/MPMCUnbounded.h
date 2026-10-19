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
            queue_.push(segment);
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

             = queue_.back();
            queue_.pop_back();
        }

        return ret;
    }

private:
    std::mutex mutex_;
    std::vector<Segment<T, MPMCBoundedLockFree>*> queue_;
    const std::size_t blockSize_;

};