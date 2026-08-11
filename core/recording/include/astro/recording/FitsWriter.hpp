#pragma once

#include <cstdint>
#include <string>

#include "astro/domain/Result.hpp"

namespace astro::recording {

// Writes a minimal single-HDU FITS file (Gate D backlog 22.8): SIMPLE/
// BITPIX/NAXIS* header cards plus END, no WCS or optional keywords — from
// `rgb8` (width*height*3 bytes, interleaved R,G,B, e.g. a debayered
// preview frame). BITPIX=8 matches the actual precision every current
// backend produces (mock's Bayer8; nothing 16-bit exists yet); the three
// channels are written band-sequential (all of R, then all of G, then all
// of B) as NAXIS3=3, the common convention for color FITS. No third-party
// dependency: FITS is a plain, fully documented binary format, well within
// reach of a hand-rolled writer for this scope.
astro::domain::Result<void> writeFits8Rgb(const std::string& path, std::uint32_t width, std::uint32_t height,
                                           const std::uint8_t* rgb8);

}  // namespace astro::recording
