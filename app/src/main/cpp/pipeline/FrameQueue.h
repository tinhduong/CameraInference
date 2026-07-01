#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include "FramePacket.h"

enum class FrameDropPolicy {
    DROP_OLDEST = 0,
    DROP_LATEST = 1
};

class FrameQueue {
public:
    explicit FrameQueue(size_t maxCapacity);
    ~FrameQueue();

    // Enqueue frame with dropping policy. Increments droppedCount on drop.
    bool enqueue(std::unique_ptr<FramePacket> frame, FrameDropPolicy policy, uint32_t& droppedCount);

    // Blocking pop. Returns nullptr if shutdown.
    std::unique_ptr<FramePacket> dequeue();

    // Non-blocking try-pop. Returns nullptr if empty or shutdown.
    std::unique_ptr<FramePacket> tryDequeue();

    void clear();
    void shutdown();
    size_t size() const;
    bool empty() const;
    void setCapacity(size_t capacity);

private:
    size_t capacity_;
    std::queue<std::unique_ptr<FramePacket>> queue_;
    mutable std::mutex mutex_;
    std::condition_variable condVar_;
    bool isShutdown_ = false;
};
