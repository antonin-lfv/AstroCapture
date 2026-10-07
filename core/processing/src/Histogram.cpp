#include "astro/processing/Histogram.hpp"
#include <cstring>
#include <algorithm>

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

// A second O(256) pass over the already-built buckets, not a second pass
// over pixel data.
void computeStats(Histogram& histogram) {
    std::uint64_t weightedSum = 0;
    std::uint64_t total = 0;
    int minIndex = -1;
    int maxIndex = -1;
    for (int i = 0; i < static_cast<int>(Histogram::kBucketCount); ++i) {
        const auto count = histogram.luminance[static_cast<std::size_t>(i)];
        if (count == 0) {
            continue;
        }
        if (minIndex == -1) {
            minIndex = i;
        }
        maxIndex = i;
        weightedSum += static_cast<std::uint64_t>(count) * static_cast<std::uint64_t>(i);
        total += count;
    }
    if (total == 0) {
        return;
    }
    histogram.minValue = static_cast<std::uint8_t>(minIndex);
    histogram.maxValue = static_cast<std::uint8_t>(maxIndex);
    histogram.meanValue = static_cast<double>(weightedSum) / static_cast<double>(total);
    histogram.hasStats = true;
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
        case PixelStorage::Mono16:
        case PixelStorage::Bayer16: {
            if(descriptor.validBits<1||descriptor.validBits>16)break;
            const auto stride=descriptor.strideBytes?descriptor.strideBytes:descriptor.width*2;
            const auto maximum=(1u<<descriptor.validBits)-1;
            for(std::uint32_t y=0;y<descriptor.height;++y){const auto offset=std::size_t(y)*stride;if(offset+std::size_t(descriptor.width)*2>size)break;for(std::uint32_t x=0;x<descriptor.width;++x){std::uint16_t sample;std::memcpy(&sample,bytes+offset+x*2,2);++histogram.luminance[std::min(255u,(unsigned(sample)*255u+maximum/2)/maximum)];}}
            break;
        }
        case PixelStorage::RGB24:
        case PixelStorage::BGR24:
        case PixelStorage::BGRA32: {
            const auto pixelBytes=descriptor.storage==PixelStorage::BGRA32?4u:3u;
            const bool reversed=descriptor.storage!=PixelStorage::RGB24;
            const std::uint32_t stride = descriptor.strideBytes != 0 ? descriptor.strideBytes : descriptor.width * pixelBytes;
            histogram.hasColorChannels = true;
            for (std::uint32_t y = 0; y < descriptor.height; ++y) {
                const std::size_t rowOffset = static_cast<std::size_t>(y) * stride;
                if (rowOffset + static_cast<std::size_t>(descriptor.width) * pixelBytes > size) {
                    break;
                }
                for (std::uint32_t x = 0; x < descriptor.width; ++x) {
                    const std::size_t pixelOffset = rowOffset + static_cast<std::size_t>(x) * pixelBytes;
                    const std::uint8_t b = bytes[pixelOffset + (reversed?0:2)];
                    const std::uint8_t g = bytes[pixelOffset + 1];
                    const std::uint8_t r = bytes[pixelOffset + (reversed?2:0)];
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

    computeStats(histogram);
    return histogram;
}

}  // namespace astro::processing
