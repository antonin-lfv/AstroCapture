#include "astro/driver/mock/BayerTestPattern.hpp"

#include <array>
#include <stdexcept>

namespace astro::driver::mock {

using astro::domain::BayerPattern;

namespace {

constexpr int kChannelR = 0;
constexpr int kChannelG = 1;
constexpr int kChannelB = 2;

// Which RGB channel sits at (x, y) for a given Bayer pattern, using the
// pattern's own 2x2 tile definition (top-left, top-right, bottom-left,
// bottom-right).
int channelAt(BayerPattern pattern, std::uint32_t x, std::uint32_t y) {
    const bool evenRow = (y % 2) == 0;
    const bool evenCol = (x % 2) == 0;

    switch (pattern) {
        case BayerPattern::RGGB:
            if (evenRow) return evenCol ? kChannelR : kChannelG;
            return evenCol ? kChannelG : kChannelB;
        case BayerPattern::BGGR:
            if (evenRow) return evenCol ? kChannelB : kChannelG;
            return evenCol ? kChannelG : kChannelR;
        case BayerPattern::GRBG:
            if (evenRow) return evenCol ? kChannelG : kChannelR;
            return evenCol ? kChannelB : kChannelG;
        case BayerPattern::GBRG:
            if (evenRow) return evenCol ? kChannelG : kChannelB;
            return evenCol ? kChannelR : kChannelG;
        case BayerPattern::None:
            break;
    }
    throw std::invalid_argument("mosaicToBayer8: pattern must not be BayerPattern::None");
}

}  // namespace

std::vector<std::uint8_t> renderColorBarsRgb(std::uint32_t width, std::uint32_t height) {
    static constexpr std::array<std::array<std::uint8_t, 3>, 4> kBars = {{
        {255, 0, 0},
        {0, 255, 0},
        {0, 0, 255},
        {255, 255, 255},
    }};

    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t barIndex = static_cast<std::size_t>(x) * kBars.size() / width;
            const auto& color = kBars[barIndex < kBars.size() ? barIndex : kBars.size() - 1];
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 3;
            rgb[offset + 0] = color[0];
            rgb[offset + 1] = color[1];
            rgb[offset + 2] = color[2];
        }
    }
    return rgb;
}

std::vector<std::uint8_t> mosaicToBayer8(const std::vector<std::uint8_t>& rgb, std::uint32_t width,
                                          std::uint32_t height, BayerPattern pattern) {
    std::vector<std::uint8_t> bayer(static_cast<std::size_t>(width) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const int channel = channelAt(pattern, x, y);
            const std::size_t rgbOffset = (static_cast<std::size_t>(y) * width + x) * 3;
            bayer[static_cast<std::size_t>(y) * width + x] = rgb[rgbOffset + static_cast<std::size_t>(channel)];
        }
    }
    return bayer;
}

}  // namespace astro::driver::mock
