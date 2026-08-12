#include "astro/driver/zwo/ZwoCameraBackend.hpp"

#include <string>
#include <string_view>

#include <ASICamera2.h>

#include "ZwoCameraDevice.hpp"

namespace astro::driver::zwo {

using namespace astro::domain;

namespace {

constexpr const char* kBackendId = "zwo";

// CameraID is only stable for as long as the camera stays connected (the
// SDK reassigns IDs starting from 0 on each enumeration) — fine for this
// project's use, since open() always immediately follows an enumerate()
// within the same rescan() call, never persisted across app restarts.
CameraDescriptor describeCamera(const ASI_CAMERA_INFO& info) {
    CameraDescriptor descriptor;
    descriptor.id = "zwo:" + std::to_string(info.CameraID);
    descriptor.displayName = info.Name;
    descriptor.backendId = kBackendId;
    return descriptor;
}

}  // namespace

std::string ZwoCameraBackend::id() const { return kBackendId; }

std::vector<CameraDescriptor> ZwoCameraBackend::enumerate() {
    std::vector<CameraDescriptor> result;
    const int count = ASIGetNumOfConnectedCameras();
    for (int i = 0; i < count; ++i) {
        ASI_CAMERA_INFO info{};
        if (ASIGetCameraProperty(&info, i) != ASI_SUCCESS) {
            continue;
        }
        result.push_back(describeCamera(info));
    }
    return result;
}

std::unique_ptr<ICameraDevice> ZwoCameraBackend::open(const CameraId& id) {
    constexpr std::string_view kPrefix = "zwo:";
    if (id.rfind(kPrefix, 0) != 0) {
        return nullptr;
    }

    int cameraId = 0;
    try {
        cameraId = std::stoi(id.substr(kPrefix.size()));
    } catch (const std::exception&) {
        return nullptr;
    }

    ASI_CAMERA_INFO info{};
    if (ASIGetCameraPropertyByID(cameraId, &info) != ASI_SUCCESS) {
        return nullptr;
    }
    if (ASIOpenCamera(cameraId) != ASI_SUCCESS) {
        return nullptr;
    }
    if (ASIInitCamera(cameraId) != ASI_SUCCESS) {
        ASICloseCamera(cameraId);
        return nullptr;
    }

    // create() takes ownership of the opened camera on every path,
    // including failure (unusable geometry) — it closes it itself.
    return ZwoCameraDevice::create(describeCamera(info), info);
}

}  // namespace astro::driver::zwo
