#pragma once

#include <qinmo/tool.h>
#include <atomic>

#if defined(__cpp_lib_hardware_interference_size) && !defined(__APPLE__)
constexpr std::size_t hardwareInterferenceSize = std::hardware_destructive_interference_size;
#else
constexpr std::size_t hardwareInterferenceSize = 64;
#endif



template <typename T>
struct StorageCell
{
    ~StorageCell()
    {
        if (sign_ & 1)
            reinterpret_cast<T*>(storage_)->~T();
    }

    template <typename... Args>
    void construct(Args&&... args) noexcept
    {
        static_assert(std::is_nothrow_constructible<T, Args&&...>::value, "T must be nothrow constructible");
        new (storage_) T(std::forward<Args>(args)...);
    }

    void destroy() noexcept
    {
        static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");
        reinterpret_cast<T*>(storage_)->~T();
    }

    T&& move() noexcept { return reinterpret_cast<T&&>(storage_); }

    alignas(hardwareInterferenceSize) std::atomic<std::size_t> sign_{0};
    alignas(T) unsigned char storage_[sizeof(T)];
};



template <typename T>
class MPMCBounded
{
public:
    MPMCBounded(std::size_t capacity)
        : capacity_(capacity)
        , head_(0)
        , tail_(0)
    {
        static_assert(
            (sizeof(StorageCell<T>) % hardwareInterferenceSize == 0) &&
            (alignof(StorageCell<T>) == hardwareInterferenceSize)    &&
            (sizeof(MPMCBounded) % hardwareInterferenceSize == 0),
            "error alignment"
        );

        if (capacity_ < 1)
            throw std::invalid_argument("capacity < 1");
        if (!qinmo::isPowerOfTwo(capacity_))
            throw std::invalid_argument("capacity must be power of 2");

        storage_ = allocator_.allocate(capacity);
        if (0 != reinterpret_cast<std::size_t>(storage_) % alignof(StorageCell<T>))
        {
            allocator_.deallocate(storage_, capacity);
            throw std::bad_alloc();
        }

        for (int i = 0; i < capacity; ++i)
            new (&storage_[i]) StorageCell<T>();
    }
    ~MPMCBounded()
    {
        for (int i = 0; i < capacity_; ++i)
            storage_[i].~StorageCell();

        allocator_.deallocate(storage_, capacity_);
    }

    MPMCBounded(const MPMCBounded&) = delete;
    MPMCBounded& operator=(const MPMCBounded&) = delete;

    template <typename... Args>
    bool emplace(Args&&... args) noexcept
    {
        static_assert(std::is_nothrow_constructible<T, Args&&...>::value, "T must be nothrow constructible with Args...");

        std::size_t currTail = tail_.load(std::memory_order_acquire);
        while (true)
        {
            StorageCell<T>& storage = storage_[index(currTail)];
            if (storage.sign_.load(std::memory_order_acquire) == turn(currTail) * 2)
            {
                if (tail_.compare_exchange_strong(currTail, currTail + 1))
                {
                    storage.construct(std::forward<Args>(args)...);
                    storage.sign_.store(turn(currTail) * 2 + 1, std::memory_order_release);
                    return true;
                }
            }
            else
            {
                std::size_t preTail = currTail;
                currTail = tail_.load(std::memory_order_acquire);

                if (currTail == preTail)
                    return false;
            }
        }
    }

    bool push(const T& val) noexcept
    {
        static_assert(std::is_nothrow_copy_constructible<T>::value, "T must be nothrow copy constructible");

        return emplace(val);
    }

    template <
        typename U,
        typename std::enable_if<
            std::is_nothrow_constructible<T, U&&>::value
        , int>::type = 0
    >
    bool push(U&& val) noexcept
    {
        return emplace(std::forward<U>(val));
    }

    bool pop(T& val) noexcept
    {
        static_assert(std::is_nothrow_assignable<T&, T&&>::value, "T must be move assignable");

        std::size_t currHead = head_.load(std::memory_order_acquire);
        while (true)
        {
            StorageCell<T>& storage = storage_[index(currHead)];
            if (storage.sign_.load(std::memory_order_acquire) == turn(currHead) * 2 + 1)
            {
                if (head_.compare_exchange_strong(currHead, currHead + 1))
                {
                    val = storage.move();
                    storage.destroy();
                    storage.sign_.store(2 * (turn(currHead) + 1), std::memory_order_release);
                    return true;
                }
            }
            else
            {
                std::size_t preHead = currHead;
                currHead = head_.load(std::memory_order_acquire);

                if (currHead == preHead)
                    return false;
            }
        }
    }

private:
    constexpr std::size_t index(std::size_t index) const noexcept { return index % capacity_; }
    constexpr std::size_t turn(std::size_t index) const noexcept { return index / capacity_; }

private:

    const std::size_t capacity_;
    StorageCell<T>* storage_;
    qinmo::allocator<StorageCell<T>> allocator_;
    alignas(hardwareInterferenceSize) std::atomic<std::size_t> head_;
    alignas(hardwareInterferenceSize) std::atomic<std::size_t> tail_;

};