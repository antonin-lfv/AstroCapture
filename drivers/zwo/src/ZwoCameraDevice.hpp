#pragma once

#include <atomic>
#include <thread>
#include <vector>

#include <ASICamera2.h>

#include "astro/domain/ICameraDevice.hpp"

namespace astro::driver::zwo {

// One opened ZWO ASI camera. Constructed only by ZwoCameraBackend::open().
// See ZwoCameraBackend.hpp's class comment: written against ZWO's
// documented SDK, compiled/linked successfully, but never run against
// real hardware — no ZWO camera was available during development.
//
// Exposes exactly one StreamMode at the sensor's full resolution
// (rounded down to the SDK's iWidth%8==0/iHeight%2==0 requirement) —
// Bayer16/Mono16 if the sensor's native bit depth is above 8 (preserves
// full dynamic range), otherwise Bayer8/Mono8. Bayer pattern comes
// directly from ASI_CAMERA_INFO — the SDK reports it, unlike
// drivers/euvc's NexImage 5 where it had to be guessed.
class ZwoCameraDevice final : public astro::domain::ICameraDevice {
public:
    // Takes ownership of an already-ASIOpenCamera()'d + ASIInitCamera()'d
    // device. Returns nullptr (closing the camera itself) if `info` (from
    // ASIGetCameraProperty) doesn't yield a usable geometry.
    static std::unique_ptr<ZwoCameraDevice> create(astro::domain::CameraDescriptor descriptor,
                                                     const ASI_CAMERA_INFO& info);

    ~ZwoCameraDevice() override;

    astro::domain::CameraDescriptor descriptor() const override;
    std::vector<astro::domain::StreamMode> streamModes() const override;
    std::vector<astro::domain::CameraCapability> capabilities() const override;

    astro::domain::Result<void> configure(const astro::domain::StreamConfiguration& configuration) override;
    astro::domain::Result<void> start(astro::domain::FrameSink& sink) override;
    astro::domain::Result<void> stop() override;

    astro::domain::Result<astro::domain::ControlValue> getControl(astro::domain::ControlId id) const override;
    astro::domain::Result<void> setControl(astro::domain::ControlId id,
                                            const astro::domain::ControlValue& value) override;

private:
    ZwoCameraDevice(astro::domain::CameraDescriptor descriptor, ASI_CAMERA_INFO info,
                     astro::domain::StreamMode mode);

    // Maps a domain ControlId to the SDK's ASI_CONTROL_TYPE. Returns false
    // (leaving `out` untouched) for ids this backend doesn't map.
    static bool controlTypeFor(astro::domain::ControlId id, ASI_CONTROL_TYPE& out);

    // Populates capabilities_ by walking ASIGetNumOfControls/
    // ASIGetControlCaps and keeping only the controls controlTypeFor()
    // recognizes — capability-driven per AGENT.md: only ever advertises
    // what this specific camera actually reports supporting, never
    // assumed.
    void probeCapabilities();

    void captureLoop();

    int cameraId_;
    ASI_CAMERA_INFO info_;
    astro::domain::CameraDescriptor descriptor_;
    astro::domain::StreamMode mode_;
    ASI_IMG_TYPE imageType_;

    std::atomic<bool> configured_{false};
    std::atomic<bool> streaming_{false};
    astro::domain::FrameSink* sink_ = nullptr;
    std::atomic<std::uint64_t> sequence_{0};
    std::thread captureThread_;

    std::vector<astro::domain::CameraCapability> capabilities_;
};

}  // namespace astro::driver::zwo
