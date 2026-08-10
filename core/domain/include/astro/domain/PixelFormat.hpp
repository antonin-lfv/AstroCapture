#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace astro::domain {

enum class BayerPattern {
    None,
    RGGB,
    BGGR,
    GRBG,
    GBRG,
};

enum class PixelStorage {
    Unknown,
    Mono8,
    Mono16,
    Bayer8,
    Bayer16,
    RGB24,
    BGR24,
    BGRA32,
};

std::string_view to_string(BayerPattern pattern);
std::string_view to_string(PixelStorage storage);

// Bytes needed to store one pixel of `storage`, ignoring any row padding.
// Returns 0 for Unknown, since callers must treat that as "cannot size a buffer".
std::size_t bytesPerPixel(PixelStorage storage);

}  // namespace astro::domain
