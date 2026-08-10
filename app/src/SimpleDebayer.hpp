#pragma once

#include <cstddef>

#include <QImage>

#include "astro/domain/Frame.hpp"

namespace astro::app {

// Fast 2x2 nearest-neighbor demosaic: each Bayer super-pixel becomes one
// flat-colored 2x2 block. This is a preview-only placeholder until the real
// debayer pipeline (astro_processing, Gate C+) exists; it trades resolution
// and quality for simplicity.
QImage debayerToPreviewImage(const astro::domain::FrameDescriptor& descriptor, const std::byte* data,
                              std::size_t size);

}  // namespace astro::app
