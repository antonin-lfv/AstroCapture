#include "astro/driver/mock/BayerTestPattern.hpp"

#include <array>
#include <stdexcept>

namespace astro::driver::mock {

using astro::domain::BayerPattern;

namespace {

// astro::domain::bayerChannelAt returns -1 for BayerPattern::None (a
// caller-error sentinel other callers might want to handle differently);
// this mosaic always has a concrete pattern, so translate that case into a
// thrown precondition failure instead of propagating -1 as a channel index.
int channelAt(BayerPattern pattern, std::uint32_t x, std::uint32_t y) {
    const int channel = astro::domain::bayerChannelAt(pattern, x, y);
    if (channel < 0) {
        throw std::invalid_argument("mosaicToBayer8: pattern must not be BayerPattern::None");
    }
    return channel;
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
