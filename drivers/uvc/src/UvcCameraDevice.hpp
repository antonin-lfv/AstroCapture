#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

#include "astro/domain/ICameraDevice.hpp"

// Objective-C++ only (this header is included solely from .mm translation
// units in this driver); AVCaptureSession/UvcSampleBufferRelay are forward
// declared as Objective-C classes, not wrapped behind void*, to keep the
// AVFoundation calls in the .mm file readable.
@class AVCaptureSession;
@class UvcSampleBufferRelay;

namespace astro::driver::uvc {

// One AVFoundation capture device. Delivers BGR24 frames (see
// UvcCameraBackend.hpp for why this can't produce raw Bayer data).
// Constructed only by UvcCameraBackend::open().
class UvcCameraDevice final : public astro::domain::ICameraDevice {
public:
    UvcCameraDevice(astro::domain::CameraDescriptor descriptor, std::string avUniqueId);
    ~UvcCameraDevice() override;

    astro::domain::CameraDescriptor descriptor() const override;
    std::vector<astro::domain::StreamMode> streamModes() const override;
    std::vector<astro::domain::CameraCapability> capabilities() const override;

    astro::domain::Result<void> configure(const astro::domain::StreamConfiguration& configuration) override;
    astro::domain::Result<void> start(astro::domain::FrameSink& sink) override;
    astro::domain::Result<void> stop() override;

    astro::domain::Result<astro::domain::ControlValue> getControl(astro::domain::ControlId id) const override;
    astro::domain::Result<void> setControl(astro::domain::ControlId id,
                                            const astro::domain::ControlValue& value) override;

    // Called by UvcSampleBufferRelay from AVFoundation's own capture queue.
    void deliverFrame(std::uint32_t width, std::uint32_t height, std::uint32_t strideBytes, const std::byte* data,
                       std::size_t size);

private:
    astro::domain::CameraDescriptor descriptor_;
    std::string avUniqueId_;

    bool configured_ = false;
    std::atomic<bool> running_{false};

    std::mutex sinkMutex_;
    astro::domain::FrameSink* sink_ = nullptr;
    std::atomic<std::uint64_t> sequence_{0};

    AVCaptureSession* session_ = nullptr;
    UvcSampleBufferRelay* relay_ = nullptr;
};

}  // namespace astro::driver::uvc
