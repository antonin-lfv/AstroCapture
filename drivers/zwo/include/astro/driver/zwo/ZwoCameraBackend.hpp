#pragma once

#include "astro/domain/ICameraBackend.hpp"

namespace astro::driver::zwo {

// Real-hardware backend for ZWO ASI cameras, wrapping ZWO's official
// ASICamera2 SDK (vendored under third_party/zwo-sdk/, MIT-licensed) —
// see docs/decisions/ADR-007-zwo-asi-backend.md. Unlike drivers/uvc and
// drivers/euvc, this backend was written and compiled/linked against the
// documented SDK but has never been exercised against real ZWO hardware
// (none was available) — treat its runtime behavior as unverified until
// tested against an actual camera.
class ZwoCameraBackend final : public astro::domain::ICameraBackend {
public:
    std::string id() const override;
    std::vector<astro::domain::CameraDescriptor> enumerate() override;
    std::unique_ptr<astro::domain::ICameraDevice> open(const astro::domain::CameraId& id) override;
};

}  // namespace astro::driver::zwo
