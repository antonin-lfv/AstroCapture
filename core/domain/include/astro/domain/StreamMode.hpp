#pragma once

#include <cstdint>

#include "astro/domain/PixelFormat.hpp"

namespace astro::domain {

// One combination of resolution/format/frame-rate range a device can stream.
struct StreamMode {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    PixelStorage storage = PixelStorage::Unknown;
    BayerPattern bayer = BayerPattern::None;
    std::uint8_t validBits = 0;
    double minFps = 0.0;
    double maxFps = 0.0;
};

struct StreamConfiguration {
    StreamMode mode;
    double targetFps = 0.0;
};

}  // namespace astro::domain
