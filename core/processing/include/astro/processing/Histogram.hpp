#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "astro/domain/Frame.hpp"

namespace astro::processing {

// A basic 256-bucket per-channel histogram of 8-bit-per-sample frame data.
// `luminance` is always populated when the format is understood. `red`/
// `green`/`blue` are only meaningful (hasColorChannels true) for formats
// with separable color channels (currently BGRA32). Bayer sources report a
// single combined luminance histogram of raw ADU values rather than
// per-color-channel counts, since separating them needs a full demosaic;
// this deliberately does not reuse FramePreviewImage's nearest-neighbor
// approach so processing stays independent/testable of the Qt-dependent
// app layer.
struct Histogram {
    static constexpr std::size_t kBucketCount = 256;

    std::array<std::uint32_t, kBucketCount> luminance{};
    std::array<std::uint32_t, kBucketCount> red{};
    std::array<std::uint32_t, kBucketCount> green{};
    std::array<std::uint32_t, kBucketCount> blue{};
    bool hasColorChannels = false;

    // Derived from `luminance` (so meaningful for Bayer/mono raw ADU
    // values and BGRA32's computed luminance alike, matching what the
    // plotted curve shows) — not per-channel, to keep this one summary
    // rather than four. Only valid when hasStats is true (at least one
    // pixel was counted; false for unrecognized formats or empty data).
    std::uint8_t minValue = 0;
    std::uint8_t maxValue = 0;
    double meanValue = 0.0;
    bool hasStats = false;
};

// Returns an all-zero Histogram for formats it doesn't understand or for
// data too short for the descriptor's claimed dimensions, rather than
// reading out of bounds.
Histogram computeHistogram(const astro::domain::FrameDescriptor& descriptor, const std::byte* data,
                            std::size_t size);

}  // namespace astro::processing
