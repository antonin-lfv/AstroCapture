#include "astro/processing/Histogram.hpp"

#include <algorithm>
#include <vector>

#include "Check.hpp"

using astro::domain::BayerPattern;
using astro::domain::FrameDescriptor;
using astro::domain::PixelStorage;
using astro::processing::computeHistogram;
using astro::processing::Histogram;

namespace {

std::vector<std::byte> toBytes(const std::vector<std::uint8_t>& values) {
    std::vector<std::byte> bytes(values.size());
    std::transform(values.begin(), values.end(), bytes.begin(), [](std::uint8_t v) { return std::byte{v}; });
    return bytes;
}

std::uint32_t sum(const std::array<std::uint32_t, Histogram::kBucketCount>& bucket) {
    std::uint32_t total = 0;
    for (const auto count : bucket) {
        total += count;
    }
    return total;
}

}  // namespace

int main() {
    // Bayer8: a 2x2 buffer with four distinct known raw values.
    {
        FrameDescriptor descriptor;
        descriptor.width = 2;
        descriptor.height = 2;
        descriptor.storage = PixelStorage::Bayer8;
        descriptor.bayer = BayerPattern::RGGB;

        const auto bytes = toBytes({10, 20, 30, 40});
        const Histogram histogram = computeHistogram(descriptor, bytes.data(), bytes.size());

        ASTRO_TEST_CHECK(!histogram.hasColorChannels);
        ASTRO_TEST_CHECK(histogram.luminance[10] == 1);
        ASTRO_TEST_CHECK(histogram.luminance[20] == 1);
        ASTRO_TEST_CHECK(histogram.luminance[30] == 1);
        ASTRO_TEST_CHECK(histogram.luminance[40] == 1);
        ASTRO_TEST_CHECK(sum(histogram.luminance) == 4);
        ASTRO_TEST_CHECK(sum(histogram.red) == 0);

        ASTRO_TEST_CHECK(histogram.hasStats);
        ASTRO_TEST_CHECK(histogram.minValue == 10);
        ASTRO_TEST_CHECK(histogram.maxValue == 40);
        ASTRO_TEST_CHECK(histogram.meanValue == 25.0);  // (10+20+30+40)/4
    }

    // BGRA32: a single pixel with known B, G, R, A bytes.
    {
        FrameDescriptor descriptor;
        descriptor.width = 1;
        descriptor.height = 1;
        descriptor.storage = PixelStorage::BGRA32;

        const auto bytes = toBytes({50, 100, 200, 255});
        const Histogram histogram = computeHistogram(descriptor, bytes.data(), bytes.size());

        ASTRO_TEST_CHECK(histogram.hasColorChannels);
        ASTRO_TEST_CHECK(histogram.blue[50] == 1);
        ASTRO_TEST_CHECK(histogram.green[100] == 1);
        ASTRO_TEST_CHECK(histogram.red[200] == 1);
        ASTRO_TEST_CHECK(histogram.luminance[(50 + 100 + 200) / 3] == 1);
    }

    // Undersized buffer: must not read past `size`, and returns whatever
    // complete rows it could read rather than crashing or fabricating data.
    {
        FrameDescriptor descriptor;
        descriptor.width = 4;
        descriptor.height = 4;
        descriptor.storage = PixelStorage::Mono8;

        const std::vector<std::byte> bytes(4);  // only one row's worth
        const Histogram histogram = computeHistogram(descriptor, bytes.data(), bytes.size());
        ASTRO_TEST_CHECK(sum(histogram.luminance) == 4);
    }

    // Unknown storage: all-zero histogram, not garbage.
    {
        FrameDescriptor descriptor;
        descriptor.width = 4;
        descriptor.height = 4;
        descriptor.storage = PixelStorage::Unknown;

        const std::vector<std::byte> bytes(16);
        const Histogram histogram = computeHistogram(descriptor, bytes.data(), bytes.size());
        ASTRO_TEST_CHECK(sum(histogram.luminance) == 0);
        ASTRO_TEST_CHECK(!histogram.hasStats);
    }

    std::cout << "OK\n";
    return 0;
}
