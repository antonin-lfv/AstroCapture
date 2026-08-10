#pragma once

#include "astro/domain/ICameraBackend.hpp"

namespace astro::driver::mock {

// Simulated backend exposing a single synthetic Bayer camera. Requires no
// hardware and no OS permissions, so it must stay usable in every build
// (dev and CI) regardless of which real backends are enabled.
class MockCameraBackend final : public astro::domain::ICameraBackend {
public:
    std::string id() const override;
    std::vector<astro::domain::CameraDescriptor> enumerate() override;
    std::unique_ptr<astro::domain::ICameraDevice> open(const astro::domain::CameraId& id) override;
};

}  // namespace astro::driver::mock
