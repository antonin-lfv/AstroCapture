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

struct DecimatedBayer {
    astro::domain::FrameDescriptor descriptor;
    std::vector<std::byte> data;
};

// Returns a reduced-resolution Bayer8 buffer for cheap preview-quality
// rendering of large sensor frames — keeps every `factor`-th 2x2 tile in
// each dimension verbatim (nearest-tile decimation, no averaging across
// skipped tiles), so the Bayer pattern stays intact for debayerBilinear()
// to run on afterward at a fraction of the pixel count. Not a
// quality-preserving downsample; only meant for on-screen preview, never
// for saved captures. `factor` must be even (2, 4, ...) to keep 2x2 tile
// alignment; 1 (or any odd value) returns the input unchanged. Returns an
// empty descriptor/data if `descriptor` isn't Bayer8 or doesn't cover
// `size`.
DecimatedBayer decimateBayer(const astro::domain::FrameDescriptor& descriptor, const std::byte* data,
                              std::size_t size, std::uint32_t factor);

}  // namespace astro::processing
