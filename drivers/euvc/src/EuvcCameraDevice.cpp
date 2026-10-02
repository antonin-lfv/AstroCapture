#include "EuvcCameraDevice.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "EuvcProtocol.hpp"

namespace astro::driver::euvc {

using namespace astro::domain;
namespace proto = protocol;

namespace {

bool fourccIs(const std::uint8_t* data, const char (&literal)[5]) {
    return std::memcmp(data, literal, 4) == 0;
}

}  // namespace

std::optional<EuvcCameraDevice::Descriptors> EuvcCameraDevice::discoverInterfaces(libusb_device_handle* handle) {
    libusb_device* dev = libusb_get_device(handle);
    libusb_config_descriptor* config = nullptr;
    if (libusb_get_active_config_descriptor(dev, &config) != 0) {
        return std::nullopt;
    }

    Descriptors result;
    int controlInterfaceNo = -1;
    for (int i = 0; i < config->bNumInterfaces; ++i) {
        const auto& iface = config->interface[i].altsetting[0];
        if (iface.bInterfaceClass == proto::kVendorInterfaceClass &&
            iface.bInterfaceSubClass == proto::kVendorInterfaceSubClass) {
            controlInterfaceNo = i;
            break;
        }
    }
    if (controlInterfaceNo < 0) {
        libusb_free_config_descriptor(config);
        return std::nullopt;
    }
    result.controlInterfaceNo = controlInterfaceNo;

    if (libusb_claim_interface(handle, controlInterfaceNo) != 0) {
        libusb_free_config_descriptor(config);
        return std::nullopt;
    }

    // Colour vs mono: needed before format matching, read via the plain
    // vendor register bank (distinct from the CT/PU control protocol).
    std::uint8_t cameraType = 0;
    if (libusb_control_transfer(handle, proto::kDirIn | proto::kTypeVendor | proto::kRecipDevice, 0x0, 0x0,
                                 proto::kRegCameraType, &cameraType, 1, proto::kUsbControlTimeoutMs) != 1) {
        libusb_release_interface(handle, controlInterfaceNo);
        libusb_free_config_descriptor(config);
        return std::nullopt;
    }
    result.isColour = (cameraType & 0x3) == 2;

    int streamInterfaceCandidate = -1;
    const auto& controlIface = config->interface[controlInterfaceNo].altsetting[0];
    const std::uint8_t* data = controlIface.extra;
    int dataLeft = controlIface.extra_length;
    while (dataLeft >= 3) {
        const std::uint8_t blockSize = data[0];
        const std::uint8_t subtype = data[2];
        if (blockSize == 0 || blockSize > dataLeft) {
            break;
        }

        if (subtype == proto::kVcHeader && blockSize > 12) {
            // Byte 12 onward lists this camera's streaming interface
            // numbers; this driver only supports/expects exactly one.
            streamInterfaceCandidate = data[12];
        } else if (subtype == proto::kVcInputTerminal && blockSize > 3) {
            result.terminalId = data[3];
        } else if (subtype == proto::kVcProcessingUnit && blockSize > 3) {
            result.processingUnitId = data[3];
        }

        dataLeft -= blockSize;
        data += blockSize;
    }

    if (streamInterfaceCandidate < 0 || result.terminalId == 0 || result.processingUnitId == 0) {
        libusb_release_interface(handle, controlInterfaceNo);
        libusb_free_config_descriptor(config);
        return std::nullopt;
    }
    result.streamInterfaceNo = streamInterfaceCandidate;

    if (result.streamInterfaceNo != result.controlInterfaceNo) {
        if (libusb_claim_interface(handle, result.streamInterfaceNo) != 0) {
            libusb_release_interface(handle, controlInterfaceNo);
            libusb_free_config_descriptor(config);
            return std::nullopt;
        }
    }

    const auto& streamIface = config->interface[result.streamInterfaceNo].altsetting[0];
    data = streamIface.extra;
    dataLeft = streamIface.extra_length;
    bool haveInterestingFormat = false;
    std::uint8_t lastFormatIndex = 0;
    while (dataLeft >= 3) {
        const std::uint8_t blockSize = data[0];
        const std::uint8_t subtype = data[2];
        if (blockSize == 0 || blockSize > dataLeft) {
            break;
        }

        if (subtype == proto::kVsInputHeader && blockSize > 6) {
            result.streamEndpoint = data[6] & 0x8f;
        } else if (subtype == proto::kVsFormatUncompressed && blockSize > 21) {
            haveInterestingFormat = false;
            if (data[21] == 16) {  // bBitsPerPixel — matches this camera family's descriptors as-is
                if (result.isColour) {
                    haveInterestingFormat = fourccIs(data + 5, "YUY2") || fourccIs(data + 5, "UYVY");
                } else {
                    haveInterestingFormat = fourccIs(data + 5, "Y800");
                }
            }
            if (haveInterestingFormat) {
                lastFormatIndex = data[3];  // bFormatIndex
            }
        } else if (subtype == proto::kVsFrameUncompressed && blockSize > 20 && haveInterestingFormat) {
            // xSize is doubled per the reference implementation's own
            // comment ("xSize value is always half what it should be") —
            // a quirk of this camera family's descriptor, not a typo here.
            const std::uint32_t width = (data[5] + (data[6] << 8)) * 2;
            const std::uint32_t height = data[7] + (data[8] << 8);
            const std::uint32_t maxBytes =
                data[17] + (data[18] << 8) + (data[19] << 16) + (static_cast<std::uint32_t>(data[20]) << 24);
            if (width * height > result.width * result.height) {
                result.width = width;
                result.height = height;
                result.maxFrameBytes = maxBytes + 2;  // +2: header slack, matches the reference implementation
                result.formatIndex = lastFormatIndex;
                result.frameIndex = data[3];  // bFrameIndex
            }
        }

        dataLeft -= blockSize;
        data += blockSize;
    }

    libusb_free_config_descriptor(config);

    if (result.streamEndpoint == 0 || result.width == 0 || result.height == 0) {
        libusb_release_interface(handle, result.controlInterfaceNo);
        if (result.streamInterfaceNo != result.controlInterfaceNo) {
            libusb_release_interface(handle, result.streamInterfaceNo);
        }
        return std::nullopt;
    }

    return result;
}

std::unique_ptr<EuvcCameraDevice> EuvcCameraDevice::create(CameraDescriptor descriptor, libusb_context* ctx,
                                                             libusb_device_handle* handle) {
    auto descriptors = discoverInterfaces(handle);
    if (!descriptors) {
        libusb_close(handle);
        libusb_exit(ctx);
        return nullptr;
    }
    return std::unique_ptr<EuvcCameraDevice>(new EuvcCameraDevice(std::move(descriptor), ctx, handle, *descriptors));
}

EuvcCameraDevice::EuvcCameraDevice(CameraDescriptor descriptor, libusb_context* ctx, libusb_device_handle* handle,
                                     Descriptors descriptors)
    : ctx_(ctx), handle_(handle), descriptor_(std::move(descriptor)), descriptors_(descriptors) {
    mode_.width = descriptors_.width;
    mode_.height = descriptors_.height;
    // See deliverFrame()'s comment: despite the "YUY2" descriptor tag, the
    // wire data is one byte per pixel — raw Bayer8 for color units.
    mode_.storage = descriptors_.isColour ? PixelStorage::Bayer8 : PixelStorage::Mono8;
    mode_.bayer = descriptors_.isColour ? BayerPattern::GBRG : BayerPattern::None;
    mode_.validBits = 8;
    mode_.minFps = 1.0;
    mode_.maxFps = 30.0;
    mode_.roiSupported = false;

    // The descriptor's own dwMaxVideoFrameBufferSize (parsed into
    // descriptors_.maxFrameBytes) was observed on this camera to report
    // exactly half the true YUY2 frame size (width*height instead of
    // width*height*2) — a device descriptor quirk, not a parsing bug (see
    // docs/development/NEXIMAGE5_HARDWARE_NOTES.md). Recomputing directly
    // from geometry avoids trusting that field for buffer sizing, since an
    // undersized transfer buffer silently truncates every payload short of
    // its end-of-frame marker, which is exactly what caused zero frames to
    // ever complete during bring-up.
    const std::uint32_t bytesPerPixel = descriptors_.isColour ? 2 : 1;
    descriptors_.maxFrameBytes =
        std::max(descriptors_.maxFrameBytes, descriptors_.width * descriptors_.height * bytesPerPixel + 4096);

    probeCapabilities();
}

EuvcCameraDevice::~EuvcCameraDevice() {
    if (streaming_) {
        stop();
    }
    libusb_release_interface(handle_, descriptors_.controlInterfaceNo);
    if (descriptors_.streamInterfaceNo != descriptors_.controlInterfaceNo) {
        libusb_release_interface(handle_, descriptors_.streamInterfaceNo);
    }
    libusb_close(handle_);
    libusb_exit(ctx_);
}

Result<std::int64_t> EuvcCameraDevice::getCtControl(std::uint8_t selector, int len, std::uint8_t request) const {
    std::uint8_t data[4] = {0, 0, 0, 0};
    const int ret = libusb_control_transfer(
        handle_, proto::kDirIn | proto::kTypeClass | proto::kRecipInterface, request,
        static_cast<std::uint16_t>(selector << 8), static_cast<std::uint16_t>(descriptors_.terminalId << 8), data,
        static_cast<std::uint16_t>(len), proto::kUsbControlTimeoutMs);
    if (ret != len) {
        return Result<std::int64_t>::failure({ErrorCode::UnsupportedControl, "Camera Terminal control read failed", ""});
    }
    std::int64_t value = 0;
    std::memcpy(&value, data, static_cast<std::size_t>(len));
    return Result<std::int64_t>::success(value);
}

Result<void> EuvcCameraDevice::setCtControl(std::uint8_t selector, int len, std::uint8_t request,
                                              std::int64_t value) {
    std::uint8_t data[4] = {0, 0, 0, 0};
    std::memcpy(data, &value, static_cast<std::size_t>(len));
    const int ret = libusb_control_transfer(
        handle_, proto::kDirOut | proto::kTypeClass | proto::kRecipInterface, request,
        static_cast<std::uint16_t>(selector << 8), static_cast<std::uint16_t>(descriptors_.terminalId << 8), data,
        static_cast<std::uint16_t>(len), proto::kUsbControlTimeoutMs);
    if (ret != len) {
        return Result<void>::failure({ErrorCode::UnsupportedControl, "Camera Terminal control write failed", ""});
    }
    return Result<void>::success();
}

Result<std::int64_t> EuvcCameraDevice::getPuControl(std::uint8_t selector, int len, std::uint8_t request) const {
    std::uint8_t data[4] = {0, 0, 0, 0};
    const int ret = libusb_control_transfer(
        handle_, proto::kDirIn | proto::kTypeClass | proto::kRecipInterface, request,
        static_cast<std::uint16_t>(selector << 8), static_cast<std::uint16_t>(descriptors_.processingUnitId << 8),
        data, static_cast<std::uint16_t>(len), proto::kUsbControlTimeoutMs);
    if (ret != len) {
        return Result<std::int64_t>::failure({ErrorCode::UnsupportedControl, "Processing Unit control read failed", ""});
    }
    std::int64_t value = 0;
    std::memcpy(&value, data, static_cast<std::size_t>(len));
    return Result<std::int64_t>::success(value);
}

Result<void> EuvcCameraDevice::setPuControl(std::uint8_t selector, int len, std::uint8_t request,
                                              std::int64_t value) {
    std::uint8_t data[4] = {0, 0, 0, 0};
    std::memcpy(data, &value, static_cast<std::size_t>(len));
    const int ret = libusb_control_transfer(
        handle_, proto::kDirOut | proto::kTypeClass | proto::kRecipInterface, request,
        static_cast<std::uint16_t>(selector << 8), static_cast<std::uint16_t>(descriptors_.processingUnitId << 8),
        data, static_cast<std::uint16_t>(len), proto::kUsbControlTimeoutMs);
    if (ret != len) {
        return Result<void>::failure({ErrorCode::UnsupportedControl, "Processing Unit control write failed", ""});
    }
    return Result<void>::success();
}

void EuvcCameraDevice::probeCapabilities() {
    // Exposure — Camera Terminal, 4 bytes, spec units of 100us (see
    // ADR-006 / kExposureTickMicroseconds-equivalent conversion below).
    const auto expCur = getCtControl(proto::kCtExposureTimeAbsoluteControl, 4, proto::kReqGetCur);
    const auto expMin = getCtControl(proto::kCtExposureTimeAbsoluteControl, 4, proto::kReqGetMin);
    const auto expMax = getCtControl(proto::kCtExposureTimeAbsoluteControl, 4, proto::kReqGetMax);
    if (expCur.ok() && expMin.ok() && expMax.ok()) {
        CameraCapability cap;
        cap.id = ControlId::Exposure;
        cap.valueType = ControlValueType::Float;
        cap.supported = true;
        cap.minValue = expMin.value() * 100.0;
        cap.maxValue = expMax.value() * 100.0;
        cap.step = 100.0;
        cap.defaultValue = expCur.value() * 100.0;
        cap.mutableWhileStreaming = true;
        cap.unit = "us";
        exposureCap_ = cap;
    }

    // Gain/Brightness — Processing Unit, 2 bytes per the real UVC spec
    // (confirmed against oaCapture's own GET-side size table, EUVCConnect.c
    // EUVCControlData[] — its SET-side code sends 4 bytes for these, which
    // looks like a harmless over-send bug there rather than the true wire
    // size; see docs/development/NEXIMAGE5_HARDWARE_NOTES.md).
    const auto gainCur = getPuControl(proto::kPuGainControl, 2, proto::kReqGetCur);
    const auto gainMin = getPuControl(proto::kPuGainControl, 2, proto::kReqGetMin);
    const auto gainMax = getPuControl(proto::kPuGainControl, 2, proto::kReqGetMax);
    if (gainCur.ok() && gainMin.ok() && gainMax.ok()) {
        CameraCapability cap;
        cap.id = ControlId::Gain;
        cap.valueType = ControlValueType::Integer;
        cap.supported = true;
        cap.minValue = static_cast<double>(gainMin.value());
        cap.maxValue = static_cast<double>(gainMax.value());
        cap.step = 1;
        cap.defaultValue = static_cast<double>(gainCur.value());
        cap.mutableWhileStreaming = true;
        gainCap_ = cap;
    }

    const auto brightCur = getPuControl(proto::kPuBrightnessControl, 2, proto::kReqGetCur);
    const auto brightMin = getPuControl(proto::kPuBrightnessControl, 2, proto::kReqGetMin);
    const auto brightMax = getPuControl(proto::kPuBrightnessControl, 2, proto::kReqGetMax);
    if (brightCur.ok() && brightMin.ok() && brightMax.ok()) {
        CameraCapability cap;
        cap.id = ControlId::Brightness;
        cap.valueType = ControlValueType::Integer;
        cap.supported = true;
        cap.minValue = static_cast<double>(brightMin.value());
        cap.maxValue = static_cast<double>(brightMax.value());
        cap.step = 1;
        cap.defaultValue = static_cast<double>(brightCur.value());
        cap.mutableWhileStreaming = true;
        brightnessCap_ = cap;
    }

    // White balance — Processing Unit, 4 bytes combined: (blue16 << 16) |
    // red16, matches both the spec and oaCapture's SET code exactly (no
    // discrepancy here, unlike gain/brightness above).
    const auto wbCur = getPuControl(proto::kPuWhiteBalanceComponentControl, 4, proto::kReqGetCur);
    const auto wbMin = getPuControl(proto::kPuWhiteBalanceComponentControl, 4, proto::kReqGetMin);
    const auto wbMax = getPuControl(proto::kPuWhiteBalanceComponentControl, 4, proto::kReqGetMax);
    if (wbCur.ok() && wbMin.ok() && wbMax.ok()) {
        const auto redOf = [](std::int64_t v) { return static_cast<double>(v & 0xffff); };
        const auto blueOf = [](std::int64_t v) { return static_cast<double>((v >> 16) & 0xffff); };
        CameraCapability cap;
        cap.valueType = ControlValueType::Integer;
        cap.supported = true;
        cap.minValue = std::min(redOf(wbMin.value()), blueOf(wbMin.value()));
        cap.maxValue = std::max(redOf(wbMax.value()), blueOf(wbMax.value()));
        cap.step = 1;
        cap.mutableWhileStreaming = true;
        cap.defaultValue = redOf(wbCur.value());
        whiteBalanceCap_ = cap;
    }
}

CameraDescriptor EuvcCameraDevice::descriptor() const { return descriptor_; }

std::vector<StreamMode> EuvcCameraDevice::streamModes() const { return {mode_}; }

std::vector<CameraCapability> EuvcCameraDevice::capabilities() const {
    std::vector<CameraCapability> caps;
    if (exposureCap_) caps.push_back(*exposureCap_);
    if (gainCap_) caps.push_back(*gainCap_);
    if (brightnessCap_) caps.push_back(*brightnessCap_);
    if (whiteBalanceCap_) {
        CameraCapability red = *whiteBalanceCap_;
        red.id = ControlId::WhiteBalanceRed;
        caps.push_back(red);
        CameraCapability blue = *whiteBalanceCap_;
        blue.id = ControlId::WhiteBalanceBlue;
        caps.push_back(blue);
    }
    return caps;
}

Result<void> EuvcCameraDevice::configure(const StreamConfiguration& configuration) {
    if (streaming_) {
        return Result<void>::failure(
            {ErrorCode::InternalInvariantViolation, "EuvcCameraDevice cannot be reconfigured while streaming", ""});
    }
    if (configuration.mode.width != mode_.width || configuration.mode.height != mode_.height) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "EuvcCameraDevice only supports its single advertised stream mode", ""});
    }

    // VS_COMMIT_CONTROL negotiation (PROBE_BLOCK, 8 bytes): bmHint(2) +
    // bFormatIndex(1) + bFrameIndex(1) + wFrameInterval(4). Format/frame
    // index aren't separately tracked (this backend only ever finds one
    // interesting format) so both are 0 here, matching what a
    // single-format device expects; wIndex=1 (not the discovered stream
    // interface number) matches the reference implementation exactly —
    // see docs/development/NEXIMAGE5_HARDWARE_NOTES.md.
    std::uint8_t probe[8] = {};
    probe[0] = 1;  // bmHint, little-endian uint16 low byte
    probe[1] = 0;
    probe[2] = descriptors_.formatIndex;
    probe[3] = descriptors_.frameIndex;
    const std::uint32_t frameInterval100ns = 333333;  // ~30fps
    std::memcpy(probe + 4, &frameInterval100ns, 4);

    const int ret = libusb_control_transfer(handle_, proto::kDirOut | proto::kTypeClass | proto::kRecipInterface,
                                             proto::kReqSetCur, 0x0200, 1, probe, sizeof(probe),
                                             proto::kUsbControlTimeoutMs);
    if (ret != sizeof(probe)) {
        return Result<void>::failure({ErrorCode::UnsupportedFormat, "Camera rejected stream format commit", ""});
    }

    configured_ = true;
    return Result<void>::success();
}

Result<void> EuvcCameraDevice::start(FrameSink& sink) {
    if (streaming_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "EuvcCameraDevice is already streaming", ""});
    }
    if (!configured_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "configure() must succeed before start()", ""});
    }

    sink_ = &sink;
    sequence_ = 0;
    frameAssembly_.clear();
    frameAssembly_.reserve(descriptors_.maxFrameBytes);
    currentFrameId_ = -1;

    transfers_.assign(kNumTransfers, nullptr);
    transferBuffers_.assign(kNumTransfers, std::vector<std::uint8_t>(descriptors_.maxFrameBytes));
    streaming_ = true;

    for (int i = 0; i < kNumTransfers; ++i) {
        libusb_transfer* transfer = libusb_alloc_transfer(0);
        if (!transfer) {
            streaming_ = false;
            cancelTransfers();
            drainTransfers();
            transferBuffers_.clear();
            sink_ = nullptr;
            return Result<void>::failure({ErrorCode::StreamStartFailed, "Failed to allocate USB transfer", ""});
        }
        transfers_[i] = transfer;
        libusb_fill_bulk_transfer(transfer, handle_, descriptors_.streamEndpoint, transferBuffers_[i].data(),
                                   static_cast<int>(transferBuffers_[i].size()), &EuvcCameraDevice::transferCallback,
                                   this, proto::kUsbBulkTimeoutMs);
        if (libusb_submit_transfer(transfer) != 0) {
            streaming_ = false;
            // This submission failed, but earlier slots are still owned
            // by libusb until their cancellation callbacks have run.
            transfers_[i] = nullptr;
            libusb_free_transfer(transfer);
            cancelTransfers();
            drainTransfers();
            transferBuffers_.clear();
            sink_ = nullptr;
            return Result<void>::failure({ErrorCode::StreamStartFailed, "Failed to submit USB transfer", ""});
        }
    }

    eventThread_ = std::thread(&EuvcCameraDevice::eventLoop, this);
    return Result<void>::success();
}

Result<void> EuvcCameraDevice::stop() {
    if (!streaming_) {
        return Result<void>::success();
    }
    streaming_ = false;
    cancelTransfers();
    if (eventThread_.joinable()) {
        eventThread_.join();
    }
    transfers_.clear();
    transferBuffers_.clear();
    sink_ = nullptr;
    return Result<void>::success();
}

void EuvcCameraDevice::cancelTransfers() {
    // Callbacks may retire slots concurrently. Keep cancellation and
    // resubmission/freeing mutually exclusive.
    std::lock_guard lock(transfersMutex_);
    for (auto* transfer : transfers_) {
        if (transfer) {
            libusb_cancel_transfer(transfer);
        }
    }
}

void EuvcCameraDevice::eventLoop() {
    timeval timeout{0, 100000};  // 100ms — bounds how promptly stop() is noticed
    while (streaming_.load(std::memory_order_relaxed)) {
        libusb_handle_events_timeout(ctx_, &timeout);
    }
    drainTransfers();
}

void EuvcCameraDevice::drainTransfers() {
    timeval timeout{0, 100000};
    // A time budget cannot authorize freeing buffers still owned by USB.
    // libusb cancellation is asynchronous: wait for every callback.
    for (;;) {
        {
            std::lock_guard lock(transfersMutex_);
            if (std::all_of(transfers_.begin(), transfers_.end(), [](auto* t) { return t == nullptr; })) {
                break;
            }
        }
        libusb_handle_events_timeout(ctx_, &timeout);
    }
}

void EuvcCameraDevice::transferCallback(libusb_transfer* transfer) {
    static_cast<EuvcCameraDevice*>(transfer->user_data)->onTransferComplete(transfer);
}

void EuvcCameraDevice::onTransferComplete(libusb_transfer* transfer) {
    if (transfer->status == LIBUSB_TRANSFER_COMPLETED && transfer->actual_length > 0) {
        const auto* buffer = static_cast<const std::uint8_t*>(transfer->buffer);
        const int len = transfer->actual_length;
        const std::uint8_t headerLength = buffer[0];
        if (headerLength <= len) {
            std::uint8_t headerInfo = headerLength >= 2 ? buffer[1] : 0;
            const bool errorBit = (headerInfo & 0x40) != 0;
            const int frameId = headerInfo & 0x1;
            const bool endOfFrame = (headerInfo & 0x2) != 0;

            if (!errorBit) {
                if (currentFrameId_ >= 0 && frameId != currentFrameId_ && !frameAssembly_.empty()) {
                    // FID toggled with no EOF seen — the previous frame is
                    // incomplete; drop it and start fresh rather than
                    // splicing two frames together.
                    frameAssembly_.clear();
                }
                currentFrameId_ = frameId;

                const int dataLength = len - headerLength;
                if (dataLength > 0 &&
                    frameAssembly_.size() + static_cast<std::size_t>(dataLength) <= descriptors_.maxFrameBytes) {
                    frameAssembly_.insert(frameAssembly_.end(), buffer + headerLength, buffer + headerLength + dataLength);
                }

                if (endOfFrame && !frameAssembly_.empty()) {
                    deliverFrame(frameAssembly_);
                    frameAssembly_.clear();
                }
            }
        }
    }

    std::lock_guard lock(transfersMutex_);
    if (streaming_.load(std::memory_order_relaxed) && transfer->status != LIBUSB_TRANSFER_CANCELLED &&
        transfer->status != LIBUSB_TRANSFER_NO_DEVICE) {
        if (libusb_submit_transfer(transfer) == 0) {
            return;
        }
    }

    // Streaming stopped (or the device is gone) — free this slot so
    // eventLoop()'s drain loop can tell when every transfer has settled.
    for (auto& slot : transfers_) {
        if (slot == transfer) {
            slot = nullptr;
        }
    }
    libusb_free_transfer(transfer);
}

void EuvcCameraDevice::deliverFrame(const std::vector<std::uint8_t>& raw) {
    if (sink_ == nullptr) {
        return;
    }

    // Despite negotiating under the "YUY2" FourCC (see discoverInterfaces),
    // live testing showed each complete (EOF-flagged) payload is exactly
    // width*height bytes — one byte per pixel, not YUY2's two. That matches
    // raw Bayer8, not chroma-subsampled color, and lines up with oaCapture's
    // own historical bug reports about *Bayer pattern* mismatches (GRBG vs
    // GBRG) for this camera family — never about YUV chroma at all. This
    // firmware appears to tag its raw sensor output "YUY2" for generic UVC
    // tooling compatibility rather than because it's actually YUV. See
    // docs/development/NEXIMAGE5_HARDWARE_NOTES.md.
    const std::size_t expectedRawBytes = static_cast<std::size_t>(mode_.width) * mode_.height;
    if (raw.size() < expectedRawBytes) {
        return;
    }

    FrameBuffer frame;
    frame.descriptor.width = mode_.width;
    frame.descriptor.height = mode_.height;
    frame.metadata.sequence = sequence_.fetch_add(1, std::memory_order_relaxed);
    frame.metadata.hostArrival = std::chrono::steady_clock::now();

    std::vector<std::byte> bytes(expectedRawBytes);
    std::memcpy(bytes.data(), raw.data(), expectedRawBytes);
    if (descriptors_.isColour) {
        frame.descriptor.storage = PixelStorage::Bayer8;
        frame.descriptor.bayer = BayerPattern::GBRG;  // best guess pending direct verification — see notes doc
        frame.descriptor.validBits = 8;
        frame.storage = std::make_shared<const BufferStorage>(std::move(bytes));
    } else {
        frame.descriptor.storage = PixelStorage::Mono8;
        frame.descriptor.bayer = BayerPattern::None;
        frame.descriptor.validBits = 8;
        frame.storage = std::make_shared<const BufferStorage>(std::move(bytes));
    }

    sink_->onFrame(frame);
}

Result<ControlValue> EuvcCameraDevice::getControl(ControlId id) const {
    switch (id) {
        case ControlId::Exposure: {
            auto raw = getCtControl(proto::kCtExposureTimeAbsoluteControl, 4, proto::kReqGetCur);
            if (!raw.ok()) return Result<ControlValue>::failure(raw.error());
            return Result<ControlValue>::success(static_cast<double>(raw.value()) * 100.0);
        }
        case ControlId::Gain: {
            auto raw = getPuControl(proto::kPuGainControl, 2, proto::kReqGetCur);
            if (!raw.ok()) return Result<ControlValue>::failure(raw.error());
            return Result<ControlValue>::success(raw.value());
        }
        case ControlId::Brightness: {
            auto raw = getPuControl(proto::kPuBrightnessControl, 2, proto::kReqGetCur);
            if (!raw.ok()) return Result<ControlValue>::failure(raw.error());
            return Result<ControlValue>::success(raw.value());
        }
        case ControlId::WhiteBalanceRed:
        case ControlId::WhiteBalanceBlue: {
            auto raw = getPuControl(proto::kPuWhiteBalanceComponentControl, 4, proto::kReqGetCur);
            if (!raw.ok()) return Result<ControlValue>::failure(raw.error());
            const std::int64_t v = raw.value();
            return Result<ControlValue>::success(id == ControlId::WhiteBalanceRed ? (v & 0xffff) : ((v >> 16) & 0xffff));
        }
        default:
            return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
    }
}

Result<void> EuvcCameraDevice::setControl(ControlId id, const ControlValue& value) {
    if (id == ControlId::Exposure) {
        const double* asDouble = std::get_if<double>(&value);
        if (asDouble == nullptr || !exposureCap_ || *asDouble < exposureCap_->minValue ||
            *asDouble > exposureCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Exposure expects a double within [min, max] microseconds", ""});
        }
        return setCtControl(proto::kCtExposureTimeAbsoluteControl, 4, proto::kReqSetCur,
                             static_cast<std::int64_t>(std::lround(*asDouble / 100.0)));
    }
    if (id == ControlId::Gain) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || !gainCap_ || *asInt < gainCap_->minValue || *asInt > gainCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Gain expects an integer within [min, max]", ""});
        }
        return setPuControl(proto::kPuGainControl, 2, proto::kReqSetCur, *asInt);
    }
    if (id == ControlId::Brightness) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || !brightnessCap_ || *asInt < brightnessCap_->minValue ||
            *asInt > brightnessCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Brightness expects an integer within [min, max]", ""});
        }
        return setPuControl(proto::kPuBrightnessControl, 2, proto::kReqSetCur, *asInt);
    }
    if (id == ControlId::WhiteBalanceRed || id == ControlId::WhiteBalanceBlue) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || !whiteBalanceCap_ || *asInt < whiteBalanceCap_->minValue ||
            *asInt > whiteBalanceCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "White balance expects an integer within [min, max]", ""});
        }
        auto current = getPuControl(proto::kPuWhiteBalanceComponentControl, 4, proto::kReqGetCur);
        if (!current.ok()) {
            return Result<void>::failure(current.error());
        }
        std::int64_t red = current.value() & 0xffff;
        std::int64_t blue = (current.value() >> 16) & 0xffff;
        (id == ControlId::WhiteBalanceRed ? red : blue) = *asInt;
        const std::int64_t combined = (blue << 16) | (red & 0xffff);
        return setPuControl(proto::kPuWhiteBalanceComponentControl, 4, proto::kReqSetCur, combined);
    }
    return Result<void>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
}

}  // namespace astro::driver::euvc
