#pragma once

#include <cstdint>
#include <string>

#include "astro/domain/Result.hpp"

namespace astro::recording {

// Writes a baseline-TIFF-6.0-compliant, uncompressed 16-bit-per-channel RGB
// TIFF (Gate D backlog 22.8) from `rgb8` (width*height*3 bytes, interleaved
// R,G,B — e.g. a debayered preview frame). Every current backend only ever
// produces 8-bit samples, so each is bit-replicated to 16 bits
// (v | v << 8): an exact, lossless upscale of the real precision, not
// fabricated extra precision — the same "8-in-16" convention other
// planetary capture tools use. Hand-rolled rather than routed through
// Qt's QImageWriter: the TIFF plugin lives in the optional qtimageformats
// module, which CI does not install, so relying on it would make this
// format silently unavailable there.
astro::domain::Result<void> writeTiff16Rgb(const std::string& path, std::uint32_t width, std::uint32_t height,
                                            const std::uint8_t* rgb8);

}  // namespace astro::recording
