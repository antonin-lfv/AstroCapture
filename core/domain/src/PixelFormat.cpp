#include "astro/domain/PixelFormat.hpp"

namespace astro::domain {

std::string_view to_string(BayerPattern pattern) {
    switch (pattern) {
        case BayerPattern::None:
            return "None";
        case BayerPattern::RGGB:
            return "RGGB";
        case BayerPattern::BGGR:
            return "BGGR";
        case BayerPattern::GRBG:
            return "GRBG";
        case BayerPattern::GBRG:
            return "GBRG";
    }
    return "Unknown";
}

std::string_view to_string(PixelStorage storage) {
    switch (storage) {
        case PixelStorage::Unknown:
            return "Unknown";
        case PixelStorage::Mono8:
            return "Mono8";
        case PixelStorage::Mono16:
            return "Mono16";
        case PixelStorage::Bayer8:
            return "Bayer8";
        case PixelStorage::Bayer16:
            return "Bayer16";
        case PixelStorage::RGB24:
            return "RGB24";
        case PixelStorage::BGR24:
            return "BGR24";
        case PixelStorage::BGRA32:
            return "BGRA32";
    }
    return "Unknown";
}

int bayerChannelAt(BayerPattern pattern, std::uint32_t x, std::uint32_t y) {
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

std::size_t bytesPerPixel(PixelStorage storage) {
    switch (storage) {
        case PixelStorage::Unknown:
            return 0;
        case PixelStorage::Mono8:
        case PixelStorage::Bayer8:
            return 1;
        case PixelStorage::Mono16:
        case PixelStorage::Bayer16:
            return 2;
        case PixelStorage::RGB24:
        case PixelStorage::BGR24:
            return 3;
        case PixelStorage::BGRA32:
            return 4;
    }
    return 0;
}

}  // namespace astro::domain
