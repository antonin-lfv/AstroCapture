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

// Which channel (0 = red, 1 = green, 2 = blue) a Bayer mosaic sample
// belongs to at pixel (x, y), given the pattern's 2x2 tile layout. The
// single place this mapping is defined — drivers/mock's synthetic mosaic
// and astro_processing's debayer both key off it, rather than each
// re-deriving the same four-way switch. Returns -1 for BayerPattern::None.
int bayerChannelAt(BayerPattern pattern, std::uint32_t x, std::uint32_t y);

}  // namespace astro::domain
