#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "astro/domain/Frame.hpp"

namespace astro::processing {

// Bilinear-interpolated RGB24 (tightly packed R,G,B bytes per pixel, no row
// padding) demosaic of a Bayer8 frame: every output pixel gets a real,
// interpolated value for all three channels — its own sampled channel plus
// the other two averaged from same-channel neighbors (orthogonal for the
// channel opposite a green sample, split orthogonal-by-channel for the two
// non-green channels at a green sample) — rather than the 2x2-tile
// nearest-neighbor replication a preview-only placeholder used before this
// existed (see app/src/FramePreviewImage.hpp's former comment). Returns an
// empty vector if `descriptor` isn't Bayer8, has no recognized Bayer
// pattern, or `data`/`size` doesn't cover its claimed dimensions.
std::vector<std::uint8_t> debayerBilinear(const astro::domain::FrameDescriptor& descriptor, const std::byte* data,
                                           std::size_t size);

}  // namespace astro::processing
