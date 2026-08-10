#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

#include "astro/domain/BufferStorage.hpp"
#include "astro/domain/PixelFormat.hpp"

namespace astro::domain {

// Describes the shape of a frame's payload. Bayer pattern and validBits are
// metadata carried alongside the buffer, never re-derived from pixel content.
struct FrameDescriptor {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // 0 means "no padding": use width * bytesPerPixel(storage).
    std::uint32_t strideBytes = 0;
    PixelStorage storage = PixelStorage::Unknown;
    BayerPattern bayer = BayerPattern::None;
    std::uint8_t validBits = 0;

    std::size_t expectedPayloadBytes() const {
        const std::uint32_t effectiveStride =
            strideBytes != 0 ? strideBytes : width * static_cast<std::uint32_t>(bytesPerPixel(storage));
        return static_cast<std::size_t>(effectiveStride) * height;
    }
};

struct FrameMetadata {
    std::uint64_t sequence = 0;
    std::chrono::steady_clock::time_point hostArrival{};
    std::optional<std::uint64_t> cameraTimestamp;
};

// A ready-to-consume frame. `storage` is immutable and safely shareable
// across preview/recording/analysis consumers for the lifetime they need.
struct FrameBuffer {
    FrameDescriptor descriptor;
    FrameMetadata metadata;
    std::shared_ptr<const BufferStorage> storage;
};

}  // namespace astro::domain
