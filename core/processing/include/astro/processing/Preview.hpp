#pragma once
#include <astro/processing/Planetary.hpp>
#include <cstdint>
namespace astro::processing {
struct PreviewTarget {std::span<std::uint8_t> pixels;std::size_t bytesPerLine{};};
void validatePreviewTarget(const FloatImage&,const PreviewTarget&);
// Full-resolution RGB888 preview only. Scientific pixels remain float32.
// Destination row padding is left untouched; callers own and retain the storage.
void renderRgb8(const FloatImage&,std::span<std::uint8_t> destination,std::size_t bytesPerLine,
                const CheckCancel& = {},ComputePreference = ComputePreference::Auto,ComputeReport* = nullptr);
}
