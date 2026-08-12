#pragma once

#include "astro/domain/ICameraBackend.hpp"

namespace astro::driver::uvc {

// Real-hardware backend for USB Video Class cameras, built on libuvc
// (talks the UVC protocol directly over libusb) rather than a per-OS native
// capture framework — see docs/decisions/ADR-004-uvc-camera-backend.md for
// why. enumerate() only reports devices advertising a raw Bayer or
// monochrome uncompressed format: consumer webcams (built-in or otherwise)
// only offer YUYV/MJPEG, so this naturally excludes them without needing a
// vendor allow-list, keeping the mock-camera dev auto-connect intact.
class UvcCameraBackend final : public astro::domain::ICameraBackend {
public:
    std::string id() const override;
    std::vector<astro::domain::CameraDescriptor> enumerate() override;
    std::unique_ptr<astro::domain::ICameraDevice> open(const astro::domain::CameraId& id) override;
};

}  // namespace astro::driver::uvc
