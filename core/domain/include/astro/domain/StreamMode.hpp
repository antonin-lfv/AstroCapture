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
    // Whether StreamConfiguration::roi is honored for this mode. False means
    // any non-empty Roi is rejected by configure() rather than ignored.
    bool roiSupported = false;
};

// A sub-rectangle of a StreamMode's full width/height to stream instead of
// the full frame, raising achievable fps on backends that read out less of
// the sensor. width == 0 means "no ROI": stream the mode's full frame.
// x/y/width/height are in the mode's full-frame pixel coordinates.
struct Roi {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct StreamConfiguration {
    StreamMode mode;
    double targetFps = 0.0;
    Roi roi;
};

}  // namespace astro::domain
