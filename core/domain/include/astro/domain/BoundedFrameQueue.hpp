#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>

#include "astro/domain/Frame.hpp"

namespace astro::domain {

// Fixed-capacity FIFO frame queue for consumers that must not silently miss
// frames (e.g. recording), unlike LatestFrameSlot's intentional overwrite
// semantics for preview. tryPush() never blocks the acquisition thread: a
// full queue means the frame is dropped and the caller is told so — it must
// count it, not swallow it (IMPLEMENTATION_BLUEPRINT.md section 9:
// "overflow is recorded as integrity failure").
class BoundedFrameQueue {
public:
    explicit BoundedFrameQueue(std::size_t capacity) : capacity_(capacity) {}

    // Returns false (frame dropped) if the queue is already at capacity.
    bool tryPush(FrameBuffer frame) {
        std::lock_guard lock(mutex_);
        if (queue_.size() >= capacity_) {
            return false;
        }
        queue_.push_back(std::move(frame));
        condition_.notify_one();
        return true;
    }

    // Blocks until a frame is available or stop() is called. Returns
    // nullopt only once stopped with nothing left queued, so callers can
    // loop `while (auto frame = queue.waitPop())` to drain everything
    // queued before stop() before exiting.
    std::optional<FrameBuffer> waitPop() {
        std::unique_lock lock(mutex_);
        condition_.wait(lock, [this] { return !queue_.empty() || stopped_; });
        if (queue_.empty()) {
            return std::nullopt;
        }
        FrameBuffer frame = std::move(queue_.front());
        queue_.pop_front();
        return frame;
    }

    // Wakes any blocked waitPop() so its loop can drain and exit cleanly.
    void stop() {
        std::lock_guard lock(mutex_);
        stopped_ = true;
        condition_.notify_all();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<FrameBuffer> queue_;
    std::size_t capacity_;
    bool stopped_ = false;
};

}  // namespace astro::domain
