#pragma once
#include <cstdint>
namespace astro::domain {
// OpenCV geometric transforms use signed 16-bit coordinates internally.
inline constexpr int maximumImageDimension = 32700;
inline constexpr std::uint64_t maximumImagePixels = 128000000;
inline bool supportedImageSize(int width, int height) {
    return width > 0 && height > 0 && width <= maximumImageDimension &&
           height <= maximumImageDimension && std::uint64_t(width) * height <= maximumImagePixels;
}
std::uint64_t physicalMemoryBytes();
std::uint64_t imageMemoryBudget();
void requireImageMemory(std::uint64_t bytes);
}
