#pragma once

#include <mutex>
#include <optional>

#include "astro/domain/Frame.hpp"

namespace astro::domain {

// Thread-safe single-slot mailbox for the most recent frame: producers
// overwrite, a consumer polling at its own pace always sees the newest
// frame and may silently skip intermediate ones. This is the acquisition
// path's bounded transport to the preview consumer (see
// IMPLEMENTATION_BLUEPRINT.md section 9); recording/analysis consumers that
// must not skip frames need their own bounded queue, not this type.
class LatestFrameSlot {
public:
    void store(FrameBuffer frame) {
        std::lock_guard lock(mutex_);
        frame_ = std::move(frame);
    }

    std::optional<FrameBuffer> latest() const {
        std::lock_guard lock(mutex_);
        return frame_;
    }

    // Drops the stored frame, if any. For consumers that disconnect a
    // device and later reconnect a different one: without this, latest()
    // would keep returning the previous camera's last frame until the new
    // device's first frame arrives.
    void clear() {
        std::lock_guard lock(mutex_);
        frame_.reset();
    }

private:
    mutable std::mutex mutex_;
    std::optional<FrameBuffer> frame_;
};

}  // namespace astro::domain
