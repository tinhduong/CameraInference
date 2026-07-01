#include "FrameQueue.h"

FrameQueue::FrameQueue(size_t maxCapacity) : capacity_(maxCapacity) {}

FrameQueue::~FrameQueue() {
    shutdown();
}

bool FrameQueue::enqueue(std::unique_ptr<FramePacket> frame, FrameDropPolicy policy, uint32_t& droppedCount) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (isShutdown_) return false;

    if (queue_.size() >= capacity_) {
        if (policy == FrameDropPolicy::DROP_LATEST) {
            droppedCount++;
            return false;
        } else { // DROP_OLDEST
            queue_.pop(); // Discard oldest
            droppedCount++;
        }
    }

    queue_.push(std::move(frame));
    lock.unlock();
    condVar_.notify_one();
    return true;
}

std::unique_ptr<FramePacket> FrameQueue::dequeue() {
    std::unique_lock<std::mutex> lock(mutex_);
    condVar_.wait(lock, [this]() { return !queue_.empty() || isShutdown_; });

    if (isShutdown_ && queue_.empty()) {
        return nullptr;
    }

    auto frame = std::move(queue_.front());
    queue_.pop();
    return frame;
}

std::unique_ptr<FramePacket> FrameQueue::tryDequeue() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty() || isShutdown_) {
        return nullptr;
    }
    auto frame = std::move(queue_.front());
    queue_.pop();
    return frame;
}

void FrameQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    while (!queue_.empty()) {
        queue_.pop();
    }
}

void FrameQueue::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        isShutdown_ = true;
    }
    condVar_.notify_all();
}

size_t FrameQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

bool FrameQueue::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

void FrameQueue::setCapacity(size_t capacity) {
    std::lock_guard<std::mutex> lock(mutex_);
    capacity_ = capacity;
}
