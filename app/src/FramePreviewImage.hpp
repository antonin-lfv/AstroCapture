#pragma once

#include <cstddef>

#include <QImage>

#include "astro/domain/Frame.hpp"

namespace astro::app {

// Converts a raw acquired frame into a QImage for preview display.
// Bayer8 goes through a fast 2x2 nearest-neighbor demosaic (preview-only
// placeholder until the real debayer pipeline exists, astro_processing,
// Gate C+). BGRA32 is wrapped directly: its in-memory byte order matches
// QImage::Format_ARGB32 on little-endian, so no conversion is needed.
// Anything else renders as black.
QImage frameToPreviewImage(const astro::domain::FrameDescriptor& descriptor, const std::byte* data,
                            std::size_t size);

}  // namespace astro::app
