#pragma once

#include <cstddef>

#include <QImage>

#include "astro/domain/Frame.hpp"

namespace astro::app {

// Converts supported mono, Bayer and RGB/BGR camera frames to an 8-bit
// display preview. Saving FITS/TIFF uses original samples separately.
QImage frameToPreviewImage(const astro::domain::FrameDescriptor& descriptor, const std::byte* data,
                            std::size_t size);

}  // namespace astro::app
