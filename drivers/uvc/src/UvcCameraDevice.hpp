#pragma once

#include <atomic>
#include <optional>

#include <libuvc/libuvc.h>

#include "UvcFormat.hpp"
#include "astro/domain/ICameraDevice.hpp"

namespace astro::driver::uvc {

// One opened UVC camera. Constructed only by UvcCameraBackend::open(), which
// hands over an already-open uvc_context_t/uvc_device_t/uvc_device_handle_t
// this instance then owns for its whole lifetime — libuvc requires the
// context to outlive any stream drawn from it, so it cannot be a short-lived
// stack local the way the mock backend's `MockCameraBackend backend;` is.
//
// Exposes exactly one StreamMode: the best raw format bestRawFormat() found
// at device-open time, at that format's native/default frame size. Multiple
// resolutions per format are not enumerated — Gate C only needs one working
// mode, and PreviewController always takes streamModes().front() regardless
// (see ADR-004's scope note on what's deferred).
class UvcCameraDevice final : public astro::domain::ICameraDevice {
public:
    // Takes ownership of ctx/dev/devh (all must be non-null and already
    // uvc_open()'d). Returns nullptr if the device offers no raw
    // Bayer/mono format (see UvcFormat.hpp) — closes/frees the handles
    // itself in that case, so the caller never has to.
    static std::unique_ptr<UvcCameraDevice> create(astro::domain::CameraDescriptor descriptor, uvc_context_t* ctx,
                                                     uvc_device_t* dev, uvc_device_handle_t* devh);

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

private:
    UvcCameraDevice(astro::domain::CameraDescriptor descriptor, uvc_context_t* ctx, uvc_device_t* dev,
                     uvc_device_handle_t* devh, RawFormatMatch format, astro::domain::StreamMode mode);

    // Best-effort: forces manual exposure / manual white balance so
    // setControl() writes actually take effect instead of being overridden
    // by the camera's own auto-exposure/auto-WB loop on the next frame.
    // Ignores failure — some devices don't implement these selectors at
    // all, which isn't fatal, just means those controls stay whatever the
    // device defaults to.
    void disableAutoModes();

    // Probes get-current on each candidate control once at construction
    // time and caches min/max/default alongside the "supported" bit,
    // matching ICameraDevice's "consult capabilities() before assuming a
    // control exists" contract without re-probing on every call.
    void probeCapabilities();

    static void frameCallback(uvc_frame_t* frame, void* userPtr);
    void onLibuvcFrame(const uvc_frame_t& frame);

    uvc_context_t* ctx_;
    uvc_device_t* dev_;
    uvc_device_handle_t* devh_;

    astro::domain::CameraDescriptor descriptor_;
    RawFormatMatch format_;
    astro::domain::StreamMode mode_;
    uvc_stream_ctrl_t streamCtrl_{};

    std::atomic<bool> configured_{false};
    std::atomic<bool> streaming_{false};
    astro::domain::FrameSink* sink_ = nullptr;
    std::atomic<std::uint64_t> sequence_{0};

    std::optional<astro::domain::CameraCapability> exposureCap_;
    std::optional<astro::domain::CameraCapability> gainCap_;
    std::optional<astro::domain::CameraCapability> gammaCap_;
    std::optional<astro::domain::CameraCapability> brightnessCap_;
    std::optional<astro::domain::CameraCapability> whiteBalanceCap_;
};

}  // namespace astro::driver::uvc
