#include "astro/processing/Debayer.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <thread>
#include <vector>

#include "astro/domain/PixelFormat.hpp"

namespace astro::processing {

using astro::domain::BayerPattern;
using astro::domain::FrameDescriptor;
using astro::domain::PixelStorage;
using astro::domain::bayerChannelAt;

namespace {

constexpr int kRed = 0;
constexpr int kGreen = 1;
constexpr int kBlue = 2;

struct Offset {
    int dx;
    int dy;
};

constexpr std::array<Offset, 4> kOrthogonal = {{{-1, 0}, {1, 0}, {0, -1}, {0, 1}}};
constexpr std::array<Offset, 4> kDiagonal = {{{-1, -1}, {1, -1}, {-1, 1}, {1, 1}}};

// Averages the samples among `offsets` (relative to x,y) whose Bayer
// channel is `wantChannel`, skipping out-of-bounds candidates. Returns the
// pixel's own value if none qualify (degenerate 1-pixel-wide/tall images).
std::uint8_t averageNeighbors(const std::uint8_t* bytes, std::uint32_t stride, std::uint32_t width,
                               std::uint32_t height, std::uint32_t x, std::uint32_t y, BayerPattern pattern,
                               const std::array<Offset, 4>& offsets, int wantChannel, std::uint8_t ownValue) {
    std::uint32_t sum = 0;
    int count = 0;
    for (const auto& offset : offsets) {
        const int nx = static_cast<int>(x) + offset.dx;
        const int ny = static_cast<int>(y) + offset.dy;
        if (nx < 0 || ny < 0 || nx >= static_cast<int>(width) || ny >= static_cast<int>(height)) {
            continue;
        }
        if (bayerChannelAt(pattern, static_cast<std::uint32_t>(nx), static_cast<std::uint32_t>(ny)) != wantChannel) {
            continue;
        }
        sum += bytes[static_cast<std::size_t>(ny) * stride + static_cast<std::size_t>(nx)];
        ++count;
    }
    return count > 0 ? static_cast<std::uint8_t>(sum / static_cast<std::uint32_t>(count)) : ownValue;
}

// Debayers rows [yBegin, yEnd) into their slice of `rgb`. Threads only ever
// read `bytes` (shared, never mutated) and write their own disjoint row
// range of `rgb`, so splitting the full image across threads by row range
// changes nothing about the result — same algorithm, same per-pixel
// neighbor averaging, just computed concurrently.
void debayerRows(const FrameDescriptor& descriptor, const std::uint8_t* bytes, std::uint32_t stride,
                  std::uint32_t width, std::uint32_t yBegin, std::uint32_t yEnd, std::uint8_t* rgb) {
    for (std::uint32_t y = yBegin; y < yEnd; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint8_t ownValue = bytes[static_cast<std::size_t>(y) * stride + x];
            const int ownChannel = bayerChannelAt(descriptor.bayer, x, y);

            std::array<std::uint8_t, 3> pixel{};
            pixel[static_cast<std::size_t>(ownChannel)] = ownValue;

            if (ownChannel == kGreen) {
                pixel[kRed] = averageNeighbors(bytes, stride, width, descriptor.height, x, y, descriptor.bayer,
                                                kOrthogonal, kRed, ownValue);
                pixel[kBlue] = averageNeighbors(bytes, stride, width, descriptor.height, x, y, descriptor.bayer,
                                                 kOrthogonal, kBlue, ownValue);
            } else {
                const int otherChannel = ownChannel == kRed ? kBlue : kRed;
                pixel[kGreen] = averageNeighbors(bytes, stride, width, descriptor.height, x, y, descriptor.bayer,
                                                  kOrthogonal, kGreen, ownValue);
                pixel[static_cast<std::size_t>(otherChannel)] =
                    averageNeighbors(bytes, stride, width, descriptor.height, x, y, descriptor.bayer, kDiagonal,
                                      otherChannel, ownValue);
            }

            const std::size_t outOffset = (static_cast<std::size_t>(y) * width + x) * 3;
            rgb[outOffset + 0] = pixel[kRed];
            rgb[outOffset + 1] = pixel[kGreen];
            rgb[outOffset + 2] = pixel[kBlue];
        }
    }
}

}  // namespace

std::vector<std::uint8_t> debayerBilinear(const FrameDescriptor& descriptor, const std::byte* data,
                                           std::size_t size) {
    if (descriptor.storage != PixelStorage::Bayer8 || descriptor.bayer == BayerPattern::None || data == nullptr) {
        return {};
    }

    const std::uint32_t width = descriptor.width;
    const std::uint32_t height = descriptor.height;
    const std::uint32_t stride = descriptor.strideBytes != 0 ? descriptor.strideBytes : width;
    if (static_cast<std::size_t>(stride) * height > size) {
        return {};
    }

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
    std::vector<std::uint8_t> rgb(static_cast<std::size_t>(width) * height * 3);

    // Bilinear debayer of a real sensor frame (multi-megapixel) is too
    // slow single-threaded to run every preview tick without blocking the
    // GUI thread (~450ms at 5MP measured during Gate C real-camera
    // bring-up, against a 50ms tick — see
    // docs/development/NEXIMAGE5_HARDWARE_NOTES.md) — split rows across
    // the available cores instead of reducing resolution/quality, so
    // captureStillImage() (which reuses this same output) isn't affected.
    const unsigned int hwThreads = std::thread::hardware_concurrency();
    const std::uint32_t threadCount = std::clamp<std::uint32_t>(hwThreads == 0 ? 4 : hwThreads, 1, height);

    if (threadCount <= 1) {
        debayerRows(descriptor, bytes, stride, width, 0, height, rgb.data());
        return rgb;
    }

    std::vector<std::thread> workers;
    workers.reserve(threadCount);
    const std::uint32_t rowsPerThread = (height + threadCount - 1) / threadCount;
    for (std::uint32_t t = 0; t < threadCount; ++t) {
        const std::uint32_t yBegin = t * rowsPerThread;
        const std::uint32_t yEnd = std::min(yBegin + rowsPerThread, height);
        if (yBegin >= yEnd) {
            break;
        }
        workers.emplace_back(debayerRows, std::cref(descriptor), bytes, stride, width, yBegin, yEnd, rgb.data());
    }
    for (auto& worker : workers) {
        worker.join();
    }

    return rgb;
}

DecimatedBayer decimateBayer(const FrameDescriptor& descriptor, const std::byte* data, std::size_t size,
                              std::uint32_t factor) {
    DecimatedBayer result;
    if (descriptor.storage != PixelStorage::Bayer8 || data == nullptr) {
        return result;
    }

    const std::uint32_t width = descriptor.width;
    const std::uint32_t height = descriptor.height;
    const std::uint32_t stride = descriptor.strideBytes != 0 ? descriptor.strideBytes : width;
    if (static_cast<std::size_t>(stride) * height > size) {
        return result;
    }

    if (factor < 2 || factor % 2 != 0) {
        result.descriptor = descriptor;
        result.data.assign(data, data + static_cast<std::size_t>(stride) * height);
        return result;
    }

    // Output dimensions in whole 2x2 tiles so the Bayer pattern stays
    // aligned in the decimated buffer.
    const std::uint32_t outWidth = (width / factor) & ~1u;
    const std::uint32_t outHeight = (height / factor) & ~1u;
    if (outWidth == 0 || outHeight == 0) {
        result.descriptor = descriptor;
        result.data.assign(data, data + static_cast<std::size_t>(stride) * height);
        return result;
    }

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
    std::vector<std::byte> out(static_cast<std::size_t>(outWidth) * outHeight);
    auto* outBytes = reinterpret_cast<std::uint8_t*>(out.data());

    for (std::uint32_t oy = 0; oy < outHeight; oy += 2) {
        const std::uint32_t srcY = oy * factor / 2 * 2;  // nearest even source row for this tile
        for (std::uint32_t ox = 0; ox < outWidth; ox += 2) {
            const std::uint32_t srcX = ox * factor / 2 * 2;
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    outBytes[static_cast<std::size_t>(oy + dy) * outWidth + (ox + dx)] =
                        bytes[static_cast<std::size_t>(srcY + dy) * stride + (srcX + dx)];
                }
            }
        }
    }

    result.descriptor = descriptor;
    result.descriptor.width = outWidth;
    result.descriptor.height = outHeight;
    result.descriptor.strideBytes = outWidth;
    result.data = std::move(out);
    return result;
}

}  // namespace astro::processing
