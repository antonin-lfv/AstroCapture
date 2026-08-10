#include "astro/driver/mock/BayerTestPattern.hpp"

#include "Check.hpp"

using astro::domain::BayerPattern;
using astro::driver::mock::mosaicToBayer8;
using astro::driver::mock::renderColorBarsRgb;

namespace {

std::uint8_t rgbAt(const std::vector<std::uint8_t>& rgb, std::uint32_t width, std::uint32_t x, std::uint32_t y,
                    int channel) {
    return rgb[(static_cast<std::size_t>(y) * width + x) * 3 + static_cast<std::size_t>(channel)];
}

std::uint8_t bayerAt(const std::vector<std::uint8_t>& bayer, std::uint32_t width, std::uint32_t x, std::uint32_t y) {
    return bayer[static_cast<std::size_t>(y) * width + x];
}

// r varies with x, g varies with y, b is constant: every pixel is
// distinguishable by which channel ended up in the mosaic.
std::vector<std::uint8_t> makeTraceableRgb(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 3;
            rgb[offset + 0] = static_cast<std::uint8_t>(x);
            rgb[offset + 1] = static_cast<std::uint8_t>(y + 10);
            rgb[offset + 2] = 99;
        }
    }
    return rgb;
}

}  // namespace

int main() {
    // --- renderColorBarsRgb: four vertical bars ---
    const auto bars = renderColorBarsRgb(8, 2);
    ASTRO_TEST_CHECK(rgbAt(bars, 8, 0, 0, 0) == 255 && rgbAt(bars, 8, 0, 0, 1) == 0 && rgbAt(bars, 8, 0, 0, 2) == 0);
    ASTRO_TEST_CHECK(rgbAt(bars, 8, 3, 0, 1) == 255);  // green bar
    ASTRO_TEST_CHECK(rgbAt(bars, 8, 5, 0, 2) == 255);  // blue bar
    ASTRO_TEST_CHECK(rgbAt(bars, 8, 7, 0, 0) == 255 && rgbAt(bars, 8, 7, 0, 1) == 255 &&
                      rgbAt(bars, 8, 7, 0, 2) == 255);  // white bar

    // --- mosaicToBayer8: every supported pattern must place the correct
    // channel at the correct pixel parity ---
    constexpr std::uint32_t kWidth = 4;
    constexpr std::uint32_t kHeight = 4;
    const auto rgb = makeTraceableRgb(kWidth, kHeight);

    {
        const auto mosaic = mosaicToBayer8(rgb, kWidth, kHeight, BayerPattern::RGGB);
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 0) == 0);    // R
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 0) == 10);   // G
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 1) == 11);   // G
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 1) == 99);   // B
    }
    {
        const auto mosaic = mosaicToBayer8(rgb, kWidth, kHeight, BayerPattern::BGGR);
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 0) == 99);  // B
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 0) == 10);  // G
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 1) == 11);  // G
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 1) == 1);   // R
    }
    {
        const auto mosaic = mosaicToBayer8(rgb, kWidth, kHeight, BayerPattern::GRBG);
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 0) == 10);  // G
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 0) == 1);   // R
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 1) == 99);  // B
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 1) == 11);  // G
    }
    {
        const auto mosaic = mosaicToBayer8(rgb, kWidth, kHeight, BayerPattern::GBRG);
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 0) == 10);  // G
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 0) == 99);  // B
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 0, 1) == 0);   // R
        ASTRO_TEST_CHECK(bayerAt(mosaic, kWidth, 1, 1) == 11);  // G
    }

    std::cout << "OK\n";
    return 0;
}
