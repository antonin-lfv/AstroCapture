#pragma once

#include "astro/domain/ICameraBackend.hpp"

namespace astro::driver::uvc {

// Generic UVC/webcam backend built on AVFoundation (macOS only). Useful for
// exercising real device discovery/open/stream/stop plumbing without owning
// the reference planetary camera (AGENT.md's Celestron NexImage 5). Webcams
// deliver already-processed BGR frames, never raw Bayer, so this backend
// cannot stand in for Gate C's color-correctness validation
// (docs/product/ROADMAP.md) — only for exercising the acquisition path.
class UvcCameraBackend final : public astro::domain::ICameraBackend {
public:
    std::string id() const override;
    std::vector<astro::domain::CameraDescriptor> enumerate() override;
    std::unique_ptr<astro::domain::ICameraDevice> open(const astro::domain::CameraId& id) override;
};

}  // namespace astro::driver::uvc
