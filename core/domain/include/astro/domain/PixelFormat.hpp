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
//
// Defined here (not in the .cpp) deliberately: astro_processing's bilinear
// debayer calls this millions of times per frame (up to 8x per pixel), and
// without LTO a cross-static-library call never gets inlined — profiling
// (`sample`) showed it as the dominant CPU cost, turning a single 640x480
// debayer into ~60ms and capping the live preview to ~16fps. Inline here
// lets every call site (in astro_processing, a different library) fold it
// away entirely.
inline int bayerChannelAt(BayerPattern pattern, std::uint32_t x, std::uint32_t y) {
    constexpr int kRed = 0;
    constexpr int kGreen = 1;
    constexpr int kBlue = 2;

    const bool evenRow = (y % 2) == 0;
    const bool evenCol = (x % 2) == 0;

    switch (pattern) {
        case BayerPattern::RGGB:
            if (evenRow) return evenCol ? kRed : kGreen;
            return evenCol ? kGreen : kBlue;
        case BayerPattern::BGGR:
            if (evenRow) return evenCol ? kBlue : kGreen;
            return evenCol ? kGreen : kRed;
        case BayerPattern::GRBG:
            if (evenRow) return evenCol ? kGreen : kRed;
            return evenCol ? kBlue : kGreen;
        case BayerPattern::GBRG:
            if (evenRow) return evenCol ? kGreen : kBlue;
            return evenCol ? kRed : kGreen;
        case BayerPattern::None:
            break;
    }
    return -1;
}

}  // namespace astro::domain
