/******************************************************************
* spsc.hpp - Single Producer Single Consumer Queue
* Author: Project 6 Team
* Last Modified: 2026-06-11
* @brief Implements multithreading without mutex via SPSC
******************************************************************/

#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace project6::supervisory
{

/**
 * @brief Fixed-capacity queue for one producer and one consumer.
 * @tparam T Stored value type.
 * @tparam Capacity Maximum queued values.
 */
template <typename T, std::size_t Capacity>
class cSpscQueue
{
    static_assert(Capacity > 0);

public:
    /** @brief Enqueues a value without blocking. */
    bool tryPush(const T& value)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t next = increment(head);
        if (next == tail_.load(std::memory_order_acquire))
        {
            return false;
        }

        storage_[head] = value;
        head_.store(next, std::memory_order_release);
        return true;
    }

    /** @brief Dequeues a value without blocking. */
    bool tryPop(T& value)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire))
        {
            return false;
        }

        value = storage_[tail];
        tail_.store(increment(tail), std::memory_order_release);
        return true;
    }

private:
    /** @brief Advances a ring-buffer index. */
    static constexpr std::size_t increment(const std::size_t index)
    {
        return (index + 1) % storageSize_;
    }

    /** Ring storage includes one sentinel slot. */
    static constexpr std::size_t storageSize_ = Capacity + 1;
    /** Preallocated value storage. */
    std::array<T, storageSize_> storage_{};
    /** Producer-owned write index. */
    std::atomic<std::size_t> head_{0};
    /** Consumer-owned read index. */
    std::atomic<std::size_t> tail_{0};
};

} // namespace project6::supervisory
