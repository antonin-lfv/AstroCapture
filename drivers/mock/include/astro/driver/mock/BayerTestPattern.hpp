#pragma once

#include <cstdint>
#include <vector>

#include "astro/domain/PixelFormat.hpp"

namespace astro::driver::mock {

// Renders a deterministic RGB24 test image: four vertical bars
// (red, green, blue, white) spanning `width` x `height`.
std::vector<std::uint8_t> renderColorBarsRgb(std::uint32_t width, std::uint32_t height);

// Mosaics an RGB24 buffer into Bayer8 according to `pattern`, i.e. the
// inverse of debayering. Used to build synthetic camera frames whose
// correct color is known ahead of time, and to regression-test that every
// supported pattern places the right channel at the right pixel. `pattern`
// must not be BayerPattern::None.
std::vector<std::uint8_t> mosaicToBayer8(const std::vector<std::uint8_t>& rgb, std::uint32_t width,
                                          std::uint32_t height, astro::domain::BayerPattern pattern);

}  // namespace astro::driver::mock
