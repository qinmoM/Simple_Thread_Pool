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
        static_assert(std::is_nothrow_constructible<T>::value, "T must be nothrow constructible");
        new (storage_) T(std::forward<Args>(args)...);
    }

    void destroy() noexcept
    {
        static_assert(std::is_nothrow_destructible<T>::value, "T must be nothrow destructible");
        reinterpret_cast<T*>(storage_)->~T();
    }

    T&& move() noexcept { return reinterpret_cast<T&&>(storage_); }

    alignas(hardwareInterferenceSize) std::atomic<std::size_t> sign_;
    alignas(T) unsigned char storage_[sizeof(T)];
};



class MPMCBounded
{
public:
private:
};