#include "astro/driver/mock/MockCameraBackend.hpp"

#include "MockCameraDevice.hpp"

namespace astro::driver::mock {

using namespace astro::domain;

namespace {

constexpr const char* kBackendId = "mock";
constexpr const char* kDeviceId = "mock:bayer-test-rggb";

CameraDescriptor makeDescriptor() {
    CameraDescriptor descriptor;
    descriptor.id = kDeviceId;
    descriptor.displayName = "Mock Camera (Bayer Test Pattern)";
    descriptor.backendId = kBackendId;
    return descriptor;
}

}  // namespace

std::string MockCameraBackend::id() const { return kBackendId; }

std::vector<CameraDescriptor> MockCameraBackend::enumerate() { return {makeDescriptor()}; }

std::unique_ptr<ICameraDevice> MockCameraBackend::open(const CameraId& id) {
    if (id != kDeviceId) {
        return nullptr;
    }
    return std::make_unique<MockCameraDevice>(makeDescriptor());
}

}  // namespace astro::driver::mock
