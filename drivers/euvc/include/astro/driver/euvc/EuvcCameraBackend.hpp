#pragma once

#include "astro/domain/ICameraBackend.hpp"

namespace astro::driver::euvc {

// Real-hardware backend for The Imaging Source / Celestron NexImage-family
// USB2 cameras in their factory "proprietary" protocol mode — see
// docs/decisions/ADR-006-euvc-proprietary-backend.md for why this exists
// alongside (not instead of) drivers/uvc: on macOS, switching this camera
// family into UVC mode makes it *less* usable (Apple's UVCAssistant claims
// UVC-class interfaces exclusively), so this talks the same UVC-spec
// control/streaming semantics directly over a vendor-specific interface
// class nothing in the OS recognizes or claims.
//
// enumerate() only reports devices exposing the expected vendor-specific
// interface (class 0xFF, subclass 1) under TIS's vendor id — generic USB
// devices (built-in webcams, unrelated peripherals) never match this, so
// no allow-list is needed to keep the mock-camera dev workflow intact.
class EuvcCameraBackend final : public astro::domain::ICameraBackend {
public:
    std::string id() const override;
    std::vector<astro::domain::CameraDescriptor> enumerate() override;
    std::unique_ptr<astro::domain::ICameraDevice> open(const astro::domain::CameraId& id) override;
};

}  // namespace astro::driver::euvc
