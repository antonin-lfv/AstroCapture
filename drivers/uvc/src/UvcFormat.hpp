#pragma once

#include <optional>

#include <libuvc/libuvc.h>

#include "astro/domain/PixelFormat.hpp"

namespace astro::driver::uvc {

struct RawFormatMatch {
    astro::domain::PixelStorage storage;
    astro::domain::BayerPattern bayer;
    uvc_frame_format libuvcFormat;
    // Owned by the device handle's format_descs list (freed on uvc_close),
    // valid for exactly as long as that handle stays open — the same
    // lifetime as the UvcCameraDevice that reads frame_descs off it.
    const uvc_format_desc_t* formatDesc = nullptr;
};

// Identifies the best raw (Bayer or monochrome) format this device
// advertises, or nullopt if it offers none — meaning it's a processed-only
// webcam (YUYV/MJPEG), not an astro-capable one. UVC uncompressed formats
// are identified by a 16-byte GUID whose first 4 bytes are the ASCII FourCC
// (the DirectShow/V4L2 convention libuvc itself relies on); comparing
// against those literals directly avoids re-deriving libuvc's own internal
// GUID table.
std::optional<RawFormatMatch> bestRawFormat(const uvc_format_desc_t* formats);

}  // namespace astro::driver::uvc
