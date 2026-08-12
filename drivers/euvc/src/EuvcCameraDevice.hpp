#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <thread>
#include <vector>

#include <libusb.h>

#include "astro/domain/ICameraDevice.hpp"

namespace astro::driver::euvc {

// One opened EUVC (TIS proprietary-protocol) camera. Constructed only by
// EuvcCameraBackend::open(), which hands over an already-open
// libusb_device_handle this instance owns for its whole lifetime — see
// docs/decisions/ADR-006-euvc-proprietary-backend.md for the protocol this
// implements and why it exists instead of (well, alongside) drivers/uvc.
//
// Exposes exactly one StreamMode, at the largest frame size the device
// advertises for its uncompressed format — same scope choice
// UvcCameraDevice made for the same reason (Gate C only needs one working
// mode; PreviewController always takes streamModes().front() anyway).
class EuvcCameraDevice final : public astro::domain::ICameraDevice {
public:
    // Takes ownership of ctx/handle (already libusb_open()'d). Returns
    // nullptr — closing/freeing them itself — if the expected
    // vendor-specific control+streaming interfaces or an uncompressed
    // format this backend understands (YUY2/UYVY color, Y800 mono) aren't
    // found.
    static std::unique_ptr<EuvcCameraDevice> create(astro::domain::CameraDescriptor descriptor, libusb_context* ctx,
                                                      libusb_device_handle* handle);

    ~EuvcCameraDevice() override;

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
    // Discovered from the device's own vendor-specific interface
    // descriptor (EuvcCameraDevice.cpp's discoverInterfaces()) — nothing
    // here is hardcoded per camera model.
    struct Descriptors {
        int controlInterfaceNo = -1;
        int streamInterfaceNo = -1;
        std::uint8_t terminalId = 0;
        std::uint8_t processingUnitId = 0;
        std::uint8_t streamEndpoint = 0;
        bool isColour = false;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t maxFrameBytes = 0;
        std::uint8_t formatIndex = 0;
        std::uint8_t frameIndex = 0;
    };

    EuvcCameraDevice(astro::domain::CameraDescriptor descriptor, libusb_context* ctx, libusb_device_handle* handle,
                      Descriptors descriptors);

    // Walks the vendor-specific control interface's class-specific
    // descriptor bytes (same layout as real UVC's VC_HEADER/
    // VC_INPUT_TERMINAL/VC_PROCESSING_UNIT) to find the streaming
    // interface, terminal/processing-unit ids, bulk endpoint and largest
    // advertised YUY2/UYVY/Y800 frame size. Returns nullopt if any of
    // that isn't found — nothing here is hardcoded per camera model.
    static std::optional<Descriptors> discoverInterfaces(libusb_device_handle* handle);

    void probeCapabilities();

    // Raw Camera Terminal / Processing Unit control transfers — see
    // EuvcProtocol.hpp. `len` is the control's wire size in bytes (UVC
    // controls are little-endian, 1/2/4 bytes depending on the selector).
    astro::domain::Result<std::int64_t> getCtControl(std::uint8_t selector, int len, std::uint8_t request) const;
    astro::domain::Result<void> setCtControl(std::uint8_t selector, int len, std::uint8_t request,
                                              std::int64_t value);
    astro::domain::Result<std::int64_t> getPuControl(std::uint8_t selector, int len, std::uint8_t request) const;
    astro::domain::Result<void> setPuControl(std::uint8_t selector, int len, std::uint8_t request,
                                              std::int64_t value);

    static void transferCallback(libusb_transfer* transfer);
    void onTransferComplete(libusb_transfer* transfer);
    void eventLoop();

    // Converts one assembled raw frame into a FrameBuffer and hands it to
    // sink_ — Bayer8 for color units, Mono8 for mono, per
    // descriptors_.isColour. The descriptor negotiates under the "YUY2"/
    // "Y800" FourCC (discoverInterfaces()'s format match), but live
    // testing showed the wire data is one byte per pixel, matching raw
    // Bayer8 rather than true YUY2 — see this function's .cpp comment and
    // docs/development/NEXIMAGE5_HARDWARE_NOTES.md.
    void deliverFrame(const std::vector<std::uint8_t>& raw);

    libusb_context* ctx_;
    libusb_device_handle* handle_;
    astro::domain::CameraDescriptor descriptor_;
    Descriptors descriptors_;
    astro::domain::StreamMode mode_;

    std::atomic<bool> configured_{false};
    std::atomic<bool> streaming_{false};
    astro::domain::FrameSink* sink_ = nullptr;
    std::atomic<std::uint64_t> sequence_{0};
    std::thread eventThread_;

    static constexpr int kNumTransfers = 8;
    std::vector<libusb_transfer*> transfers_;
    std::vector<std::vector<std::uint8_t>> transferBuffers_;

    // Per-frame reassembly state, touched only from the libusb event
    // thread (transferCallback/onTransferComplete) — never from the GUI
    // thread, so no lock needed.
    std::vector<std::uint8_t> frameAssembly_;
    int currentFrameId_ = -1;

    std::optional<astro::domain::CameraCapability> exposureCap_;
    std::optional<astro::domain::CameraCapability> gainCap_;
    std::optional<astro::domain::CameraCapability> gammaCap_;
    std::optional<astro::domain::CameraCapability> brightnessCap_;
    std::optional<astro::domain::CameraCapability> whiteBalanceCap_;
};

}  // namespace astro::driver::euvc
