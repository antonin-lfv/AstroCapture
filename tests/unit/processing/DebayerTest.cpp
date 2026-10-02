#include "astro/processing/Debayer.hpp"

#include <algorithm>
#include <array>
#include <vector>
#include <random>

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
    // Compare every pixel to an independent scalar 3x3 neighborhood
    // reference, including borders, odd dimensions, padding and tiny ROIs.
    std::mt19937 random(42);
    for (const auto pattern : kPatterns) {
        for (const auto width : {1u, 2u, 7u, 65u, 1024u}) {
            const std::uint32_t height = width == 1024 ? 768 : 9;
            FrameDescriptor d{width, height, width + 3, PixelStorage::Bayer8, pattern, 8};
            std::vector<std::byte> raw(d.expectedPayloadBytes());
            for (auto& value : raw) value = std::byte(random() % 256);
            const auto actual = debayerBilinear(d, raw.data(), raw.size());
            ASTRO_TEST_CHECK(actual.size() == static_cast<std::size_t>(width) * height * 3);
            for (std::uint32_t y = 0; y < height; ++y) {
                for (std::uint32_t x = 0; x < width; ++x) {
                    const auto own = std::to_integer<int>(raw[y * d.strideBytes + x]);
                    for (int channel = 0; channel < 3; ++channel) {
                        int sum = 0, count = 0;
                        for (int dy = -1; dy <= 1; ++dy) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                const int nx = static_cast<int>(x) + dx, ny = static_cast<int>(y) + dy;
                                if (nx >= 0 && ny >= 0 && nx < static_cast<int>(width) && ny < static_cast<int>(height) &&
                                    bayerChannelAt(pattern, nx, ny) == channel) {
                                    sum += std::to_integer<int>(raw[ny * d.strideBytes + nx]);
                                    ++count;
                                }
                            }
                        }
                        const int expected = channel == bayerChannelAt(pattern, x, y) ? own : (count ? sum / count : own);
                        ASTRO_TEST_CHECK(actual[(static_cast<std::size_t>(y) * width + x) * 3 + channel] == expected);
                    }
                }
            }
        }
    }

    // Reject invalid geometry/patterns before starting any worker or
    // reading pixels, including a stride shorter than a row.
    {
        FrameDescriptor d{4, 4, 2, PixelStorage::Bayer8, BayerPattern::GBRG, 8};
        const std::vector<std::byte> raw(16);
        ASTRO_TEST_CHECK(debayerBilinear(d, raw.data(), raw.size()).empty());
        ASTRO_TEST_CHECK(astro::processing::decimateBayer(d, raw.data(), raw.size(), 2).data.empty());
        d.strideBytes = 4;
        d.height = 0;
        ASTRO_TEST_CHECK(debayerBilinear(d, raw.data(), raw.size()).empty());
        d.height = 4;
        d.bayer = static_cast<BayerPattern>(99);
        ASTRO_TEST_CHECK(debayerBilinear(d, raw.data(), raw.size()).empty());
    }

    // Decimation must preserve every channel's parity, including odd edges.
    for (const auto pattern : kPatterns) {
        FrameDescriptor d{67, 19, 70, PixelStorage::Bayer8, pattern, 8};
        std::vector<std::byte> raw(d.expectedPayloadBytes());
        for (std::uint32_t y = 0; y < d.height; ++y)
            for (std::uint32_t x = 0; x < d.width; ++x)
                raw[y * d.strideBytes + x] = std::byte(40 + 60 * bayerChannelAt(pattern, x, y));
        for (auto factor : {2u, 4u, 8u}) {
            const auto result = astro::processing::decimateBayer(d, raw.data(), raw.size(), factor);
            ASTRO_TEST_CHECK(result.descriptor.bayer == pattern);
            for (std::uint32_t y = 0; y < result.descriptor.height; ++y)
                for (std::uint32_t x = 0; x < result.descriptor.width; ++x)
                    ASTRO_TEST_CHECK(std::to_integer<int>(result.data[y * result.descriptor.strideBytes + x]) ==
                                     40 + 60 * bayerChannelAt(pattern, x, y));
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
