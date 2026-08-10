#include "astro/processing/Histogram.hpp"

namespace astro::processing {

using astro::domain::FrameDescriptor;
using astro::domain::PixelStorage;

namespace {

void accumulateSingleChannel(const std::uint8_t* bytes, std::uint32_t count,
                              std::array<std::uint32_t, Histogram::kBucketCount>& bucket) {
    for (std::uint32_t i = 0; i < count; ++i) {
        ++bucket[bytes[i]];
    }
}

}  // namespace

Histogram computeHistogram(const FrameDescriptor& descriptor, const std::byte* data, std::size_t size) {
    Histogram histogram;
    if (data == nullptr) {
        return histogram;
    }

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);

    switch (descriptor.storage) {
        case PixelStorage::Mono8:
        case PixelStorage::Bayer8: {
            const std::uint32_t stride = descriptor.strideBytes != 0 ? descriptor.strideBytes : descriptor.width;
            for (std::uint32_t y = 0; y < descriptor.height; ++y) {
                const std::size_t rowOffset = static_cast<std::size_t>(y) * stride;
                if (rowOffset + descriptor.width > size) {
                    break;
                }
                accumulateSingleChannel(bytes + rowOffset, descriptor.width, histogram.luminance);
            }
            break;
        }
        case PixelStorage::BGRA32: {
            const std::uint32_t stride =
                descriptor.strideBytes != 0 ? descriptor.strideBytes : descriptor.width * 4;
            histogram.hasColorChannels = true;
            for (std::uint32_t y = 0; y < descriptor.height; ++y) {
                const std::size_t rowOffset = static_cast<std::size_t>(y) * stride;
                if (rowOffset + static_cast<std::size_t>(descriptor.width) * 4 > size) {
                    break;
                }
                for (std::uint32_t x = 0; x < descriptor.width; ++x) {
                    const std::size_t pixelOffset = rowOffset + static_cast<std::size_t>(x) * 4;
                    const std::uint8_t b = bytes[pixelOffset + 0];
                    const std::uint8_t g = bytes[pixelOffset + 1];
                    const std::uint8_t r = bytes[pixelOffset + 2];
                    ++histogram.blue[b];
                    ++histogram.green[g];
                    ++histogram.red[r];
                    ++histogram.luminance[static_cast<std::uint8_t>((static_cast<int>(r) + g + b) / 3)];
                }
            }
            break;
        }
        default:
            break;
    }

    return histogram;
}

}  // namespace astro::processing
