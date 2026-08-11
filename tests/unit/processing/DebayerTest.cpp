#include "astro/processing/Debayer.hpp"

#include <algorithm>
#include <array>
#include <vector>

#include "astro/driver/mock/BayerTestPattern.hpp"

#include "Check.hpp"

using namespace astro::domain;
using astro::driver::mock::mosaicToBayer8;
using astro::driver::mock::renderColorBarsRgb;
using astro::processing::debayerBilinear;

namespace {

std::vector<std::byte> toBytes(const std::vector<std::uint8_t>& values) {
    std::vector<std::byte> bytes(values.size());
    std::transform(values.begin(), values.end(), bytes.begin(), [](std::uint8_t v) { return std::byte{v}; });
    return bytes;
}

void checkColorAt(const std::vector<std::uint8_t>& rgb, std::uint32_t width, std::uint32_t x, std::uint32_t y,
                   std::uint8_t expectedR, std::uint8_t expectedG, std::uint8_t expectedB) {
    const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 3;
    ASTRO_TEST_CHECK(rgb[offset + 0] == expectedR);
    ASTRO_TEST_CHECK(rgb[offset + 1] == expectedG);
    ASTRO_TEST_CHECK(rgb[offset + 2] == expectedB);
}

}  // namespace

// Section 20's "test-first Bayer investigation": a synthetic image with
// known solid red/green/blue/white patches, mosaicked into every supported
// Bayer pattern, must debayer back to those same dominant colors — the
// exact class of color-channel-swap bug that motivated this project.
int main() {
    constexpr std::uint32_t kWidth = 64;  // 4 bars x 16px: wide enough for an interior test column per bar
    constexpr std::uint32_t kHeight = 6;

    const auto colorBarsRgb = renderColorBarsRgb(kWidth, kHeight);

    constexpr std::array<BayerPattern, 4> kPatterns = {BayerPattern::RGGB, BayerPattern::BGGR, BayerPattern::GRBG,
                                                         BayerPattern::GBRG};
    for (const auto pattern : kPatterns) {
        const auto mosaic = mosaicToBayer8(colorBarsRgb, kWidth, kHeight, pattern);
        const auto bytes = toBytes(mosaic);

        FrameDescriptor descriptor;
        descriptor.width = kWidth;
        descriptor.height = kHeight;
        descriptor.storage = PixelStorage::Bayer8;
        descriptor.bayer = pattern;

        const auto rgb = debayerBilinear(descriptor, bytes.data(), bytes.size());
        ASTRO_TEST_CHECK(rgb.size() == static_cast<std::size_t>(kWidth) * kHeight * 3);

        // Interior columns of each 16px bar, well away from the x=16/32/48
        // bar boundaries where bilinear interpolation legitimately blends
        // two colors, must reconstruct exactly: every same-channel
        // neighbor within a solid-color region carries the same value the
        // mosaic started from.
        for (std::uint32_t y = 0; y < kHeight; ++y) {
            checkColorAt(rgb, kWidth, 6, y, 255, 0, 0);       // red bar
            checkColorAt(rgb, kWidth, 22, y, 0, 255, 0);      // green bar
            checkColorAt(rgb, kWidth, 38, y, 0, 0, 255);      // blue bar
            checkColorAt(rgb, kWidth, 54, y, 255, 255, 255);  // white bar
        }
    }

    // Unsupported storage: empty result, not a garbage reinterpretation.
    {
        FrameDescriptor descriptor;
        descriptor.width = 4;
        descriptor.height = 4;
        descriptor.storage = PixelStorage::BGRA32;
        const std::vector<std::byte> bytes(64);
        ASTRO_TEST_CHECK(debayerBilinear(descriptor, bytes.data(), bytes.size()).empty());
    }

    // Undersized buffer: empty result, not an out-of-bounds read.
    {
        FrameDescriptor descriptor;
        descriptor.width = 4;
        descriptor.height = 4;
        descriptor.storage = PixelStorage::Bayer8;
        descriptor.bayer = BayerPattern::RGGB;
        const std::vector<std::byte> tooSmall(4);  // needs 16 bytes
        ASTRO_TEST_CHECK(debayerBilinear(descriptor, tooSmall.data(), tooSmall.size()).empty());
    }

    std::cout << "OK\n";
    return 0;
}
