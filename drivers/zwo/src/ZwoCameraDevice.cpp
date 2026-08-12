#include "ZwoCameraDevice.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace astro::driver::zwo {

using namespace astro::domain;

namespace {

BayerPattern toBayerPattern(ASI_BAYER_PATTERN pattern) {
    switch (pattern) {
        case ASI_BAYER_RG:
            return BayerPattern::RGGB;
        case ASI_BAYER_BG:
            return BayerPattern::BGGR;
        case ASI_BAYER_GR:
            return BayerPattern::GRBG;
        case ASI_BAYER_GB:
            return BayerPattern::GBRG;
        default:
            return BayerPattern::None;
    }
}

StreamMode buildMode(const ASI_CAMERA_INFO& info, ASI_IMG_TYPE& imageType) {
    StreamMode mode;
    // SDK constraints (ASICamera2.h ASISetROIFormat doc): iWidth%8==0,
    // iHeight%2==0. Real ASI sensor resolutions are documented to already
    // satisfy this, but rounding down defensively costs nothing.
    mode.width = (static_cast<std::uint32_t>(info.MaxWidth) / 8) * 8;
    mode.height = (static_cast<std::uint32_t>(info.MaxHeight) / 2) * 2;

    const bool isColor = info.IsColorCam == ASI_TRUE;
    // BitDepth above 8 keeps full dynamic range in a 16-bit container;
    // ASI_IMG_RAW8/RAW16 both deliver genuine raw sensor data (unlike
    // RGB24, which the SDK would demosaic on-camera) — see ADR-007.
    const bool wide = info.BitDepth > 8;
    if (isColor) {
        mode.storage = wide ? PixelStorage::Bayer16 : PixelStorage::Bayer8;
        mode.bayer = toBayerPattern(info.BayerPattern);
    } else {
        mode.storage = wide ? PixelStorage::Mono16 : PixelStorage::Mono8;
        mode.bayer = BayerPattern::None;
    }
    imageType = wide ? ASI_IMG_RAW16 : ASI_IMG_RAW8;

    mode.validBits = static_cast<std::uint8_t>(info.BitDepth > 0 ? info.BitDepth : 8);
    mode.minFps = 1.0;
    mode.maxFps = 60.0;  // Not reported by the SDK per mode; a practical cap, not a hardware limit.
    mode.roiSupported = false;
    return mode;
}

}  // namespace

bool ZwoCameraDevice::controlTypeFor(ControlId id, ASI_CONTROL_TYPE& out) {
    switch (id) {
        case ControlId::Exposure:
            out = ASI_EXPOSURE;
            return true;
        case ControlId::Gain:
            out = ASI_GAIN;
            return true;
        case ControlId::Gamma:
            out = ASI_GAMMA;
            return true;
        case ControlId::WhiteBalanceRed:
            out = ASI_WB_R;
            return true;
        case ControlId::WhiteBalanceBlue:
            out = ASI_WB_B;
            return true;
        case ControlId::Brightness:
            out = ASI_OFFSET;
            return true;
        default:
            return false;
    }
}

std::unique_ptr<ZwoCameraDevice> ZwoCameraDevice::create(CameraDescriptor descriptor, const ASI_CAMERA_INFO& info) {
    ASI_IMG_TYPE imageType = ASI_IMG_RAW8;
    StreamMode mode = buildMode(info, imageType);
    if (mode.width == 0 || mode.height == 0) {
        ASICloseCamera(info.CameraID);
        return nullptr;
    }
    return std::unique_ptr<ZwoCameraDevice>(new ZwoCameraDevice(std::move(descriptor), info, mode));
}

ZwoCameraDevice::ZwoCameraDevice(CameraDescriptor descriptor, ASI_CAMERA_INFO info, StreamMode mode)
    : cameraId_(info.CameraID), info_(info), descriptor_(std::move(descriptor)), mode_(mode) {
    imageType_ = mode_.validBits > 8 ? ASI_IMG_RAW16 : ASI_IMG_RAW8;
    probeCapabilities();
}

ZwoCameraDevice::~ZwoCameraDevice() {
    if (streaming_) {
        stop();
    }
    ASICloseCamera(cameraId_);
}

void ZwoCameraDevice::probeCapabilities() {
    int numControls = 0;
    if (ASIGetNumOfControls(cameraId_, &numControls) != ASI_SUCCESS) {
        return;
    }

    for (int i = 0; i < numControls; ++i) {
        ASI_CONTROL_CAPS caps{};
        if (ASIGetControlCaps(cameraId_, i, &caps) != ASI_SUCCESS) {
            continue;
        }

        ControlId matched;
        bool recognized = false;
        switch (caps.ControlType) {
            case ASI_EXPOSURE:
                matched = ControlId::Exposure;
                recognized = true;
                break;
            case ASI_GAIN:
                matched = ControlId::Gain;
                recognized = true;
                break;
            case ASI_GAMMA:
                matched = ControlId::Gamma;
                recognized = true;
                break;
            case ASI_WB_R:
                matched = ControlId::WhiteBalanceRed;
                recognized = true;
                break;
            case ASI_WB_B:
                matched = ControlId::WhiteBalanceBlue;
                recognized = true;
                break;
            case ASI_OFFSET:
                matched = ControlId::Brightness;
                recognized = true;
                break;
            default:
                break;
        }
        if (!recognized || !caps.IsWritable) {
            continue;
        }

        CameraCapability capability;
        capability.id = matched;
        capability.valueType = matched == ControlId::Exposure ? ControlValueType::Float : ControlValueType::Integer;
        capability.supported = true;
        capability.readOnly = false;
        capability.minValue = static_cast<double>(caps.MinValue);
        capability.maxValue = static_cast<double>(caps.MaxValue);
        capability.step = 1.0;
        capability.defaultValue = static_cast<double>(caps.DefaultValue);
        capability.supportsAuto = caps.IsAutoSupported == ASI_TRUE;
        capability.mutableWhileStreaming = true;
        if (matched == ControlId::Exposure) {
            capability.unit = "us";
        }
        capabilities_.push_back(capability);
    }
}

CameraDescriptor ZwoCameraDevice::descriptor() const { return descriptor_; }

std::vector<StreamMode> ZwoCameraDevice::streamModes() const { return {mode_}; }

std::vector<CameraCapability> ZwoCameraDevice::capabilities() const { return capabilities_; }

Result<void> ZwoCameraDevice::configure(const StreamConfiguration& configuration) {
    if (streaming_) {
        return Result<void>::failure(
            {ErrorCode::InternalInvariantViolation, "ZwoCameraDevice cannot be reconfigured while streaming", ""});
    }
    if (configuration.mode.width != mode_.width || configuration.mode.height != mode_.height) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "ZwoCameraDevice only supports its single advertised stream mode", ""});
    }

    const auto err = ASISetROIFormat(cameraId_, static_cast<int>(mode_.width), static_cast<int>(mode_.height),
                                      /*iBin=*/1, imageType_);
    if (err != ASI_SUCCESS) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "Camera rejected ROI/format configuration", std::to_string(err)});
    }

    configured_ = true;
    return Result<void>::success();
}

Result<void> ZwoCameraDevice::start(FrameSink& sink) {
    if (streaming_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "ZwoCameraDevice is already streaming", ""});
    }
    if (!configured_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "configure() must succeed before start()", ""});
    }

    const auto err = ASIStartVideoCapture(cameraId_);
    if (err != ASI_SUCCESS) {
        return Result<void>::failure(
            {ErrorCode::StreamStartFailed, "Failed to start ASI video capture", std::to_string(err)});
    }

    sink_ = &sink;
    sequence_ = 0;
    streaming_ = true;
    captureThread_ = std::thread(&ZwoCameraDevice::captureLoop, this);
    return Result<void>::success();
}

Result<void> ZwoCameraDevice::stop() {
    if (!streaming_) {
        return Result<void>::success();
    }
    streaming_ = false;
    // Stop capture before joining: captureLoop() may be blocked inside
    // ASIGetVideoData's wait, and stopping capture is what the SDK
    // documents as the way to make a pending call return (see
    // ASIGetVideoData's own doc comment on being safe to call in a loop
    // from another thread — the suggested-procedure comment at the top of
    // ASICamera2.h shows this exact pattern).
    ASIStopVideoCapture(cameraId_);
    if (captureThread_.joinable()) {
        captureThread_.join();
    }
    sink_ = nullptr;
    return Result<void>::success();
}

void ZwoCameraDevice::captureLoop() {
    const std::size_t bytesPerPixel = mode_.validBits > 8 ? 2 : 1;
    std::vector<std::byte> buffer(static_cast<std::size_t>(mode_.width) * mode_.height * bytesPerPixel);

    while (streaming_.load(std::memory_order_relaxed)) {
        long exposureUs = 0;
        ASI_BOOL isAuto = ASI_FALSE;
        ASIGetControlValue(cameraId_, ASI_EXPOSURE, &exposureUs, &isAuto);
        // SDK's own recommendation (ASIGetVideoData doc): exposure*2+500ms.
        const int waitMs = std::clamp(static_cast<int>(exposureUs / 1000) * 2 + 500, 1000, 65000);

        const auto err = ASIGetVideoData(cameraId_, reinterpret_cast<unsigned char*>(buffer.data()),
                                          static_cast<long>(buffer.size()), waitMs);
        if (err == ASI_SUCCESS && sink_ != nullptr) {
            FrameBuffer frame;
            frame.descriptor.width = mode_.width;
            frame.descriptor.height = mode_.height;
            frame.descriptor.storage = mode_.storage;
            frame.descriptor.bayer = mode_.bayer;
            frame.descriptor.validBits = mode_.validBits;
            frame.metadata.sequence = sequence_.fetch_add(1, std::memory_order_relaxed);
            frame.metadata.hostArrival = std::chrono::steady_clock::now();
            frame.storage = std::make_shared<const BufferStorage>(buffer);
            sink_->onFrame(frame);
        }
        // ASI_ERROR_TIMEOUT is expected/routine (no frame ready yet within
        // waitMs) — just loop again. Any other error also just retries;
        // ASIStopVideoCapture (called from stop()) is what ends the loop.
    }
}

Result<ControlValue> ZwoCameraDevice::getControl(ControlId id) const {
    ASI_CONTROL_TYPE controlType;
    if (!controlTypeFor(id, controlType)) {
        return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
    }
    long value = 0;
    ASI_BOOL isAuto = ASI_FALSE;
    if (ASIGetControlValue(cameraId_, controlType, &value, &isAuto) != ASI_SUCCESS) {
        return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Failed to read control", ""});
    }
    if (id == ControlId::Exposure) {
        return Result<ControlValue>::success(static_cast<double>(value));
    }
    return Result<ControlValue>::success(static_cast<std::int64_t>(value));
}

Result<void> ZwoCameraDevice::setControl(ControlId id, const ControlValue& value) {
    ASI_CONTROL_TYPE controlType;
    if (!controlTypeFor(id, controlType)) {
        return Result<void>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
    }

    const auto capability = std::find_if(capabilities_.begin(), capabilities_.end(),
                                          [id](const CameraCapability& c) { return c.id == id; });
    if (capability == capabilities_.end()) {
        return Result<void>::failure({ErrorCode::UnsupportedControl, "Control not supported by this camera", ""});
    }

    long raw = 0;
    if (id == ControlId::Exposure) {
        const double* asDouble = std::get_if<double>(&value);
        if (asDouble == nullptr || *asDouble < capability->minValue || *asDouble > capability->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Exposure expects a double within [min, max] microseconds", ""});
        }
        raw = static_cast<long>(std::lround(*asDouble));
    } else {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || *asInt < capability->minValue || *asInt > capability->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Control expects an integer within [min, max]", ""});
        }
        raw = static_cast<long>(*asInt);
    }

    if (ASISetControlValue(cameraId_, controlType, raw, ASI_FALSE) != ASI_SUCCESS) {
        return Result<void>::failure({ErrorCode::UnsupportedControl, "Camera rejected control value", ""});
    }
    return Result<void>::success();
}

}  // namespace astro::driver::zwo
