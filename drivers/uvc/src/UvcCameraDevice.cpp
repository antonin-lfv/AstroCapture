#include "UvcCameraDevice.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace astro::driver::uvc {

using namespace astro::domain;

namespace {

// UVC exposure-time-absolute is in 100us ticks (dwExposureTimeAbsolute,
// UVC spec A.9.4) — the domain's Exposure control is always microseconds
// (matches drivers/mock), so every raw tick value crossing the boundary
// goes through this.
constexpr double kExposureTickMicroseconds = 100.0;

// Frame intervals are 100ns units (UVC spec 2.4.2.3); fps = 1 / (interval *
// 100ns).
double fpsFromInterval(std::uint32_t interval100ns) {
    return interval100ns > 0 ? 10'000'000.0 / static_cast<double>(interval100ns) : 0.0;
}

const uvc_frame_desc_t* largestFrameDesc(const uvc_format_desc_t& formatDesc) {
    const uvc_frame_desc_t* best = nullptr;
    for (const uvc_frame_desc_t* frame = formatDesc.frame_descs; frame != nullptr; frame = frame->next) {
        if (best == nullptr ||
            static_cast<std::uint32_t>(frame->wWidth) * frame->wHeight >
                static_cast<std::uint32_t>(best->wWidth) * best->wHeight) {
            best = frame;
        }
    }
    return best;
}

void frameFpsRange(const uvc_frame_desc_t& frame, double& minFps, double& maxFps) {
    if (frame.bFrameIntervalType == 0) {
        // Continuous range.
        maxFps = fpsFromInterval(frame.dwMinFrameInterval);
        minFps = fpsFromInterval(frame.dwMaxFrameInterval);
        return;
    }
    // Discrete, zero-terminated list of intervals.
    std::uint32_t shortest = frame.dwDefaultFrameInterval;
    std::uint32_t longest = frame.dwDefaultFrameInterval;
    bool any = false;
    for (const std::uint32_t* interval = frame.intervals; interval != nullptr && *interval != 0; ++interval) {
        shortest = any ? std::min(shortest, *interval) : *interval;
        longest = any ? std::max(longest, *interval) : *interval;
        any = true;
    }
    maxFps = fpsFromInterval(shortest);
    minFps = fpsFromInterval(longest);
}

StreamMode buildMode(const RawFormatMatch& format) {
    StreamMode mode;
    if (const uvc_frame_desc_t* frame = largestFrameDesc(*format.formatDesc); frame != nullptr) {
        mode.width = frame->wWidth;
        mode.height = frame->wHeight;
        frameFpsRange(*frame, mode.minFps, mode.maxFps);
    }
    mode.storage = format.storage;
    mode.bayer = format.bayer;
    mode.validBits = format.storage == PixelStorage::Mono16 ? 16 : 8;
    mode.roiSupported = false;
    return mode;
}

}  // namespace

std::unique_ptr<UvcCameraDevice> UvcCameraDevice::create(CameraDescriptor descriptor, uvc_context_t* ctx,
                                                           uvc_device_t* dev, uvc_device_handle_t* devh) {
    const auto format = bestRawFormat(uvc_get_format_descs(devh));
    if (!format || format->formatDesc == nullptr) {
        uvc_close(devh);
        uvc_unref_device(dev);
        uvc_exit(ctx);
        return nullptr;
    }

    StreamMode mode = buildMode(*format);
    if (mode.width == 0 || mode.height == 0) {
        uvc_close(devh);
        uvc_unref_device(dev);
        uvc_exit(ctx);
        return nullptr;
    }

    // std::unique_ptr<T>::reset requires a public constructor; make_unique
    // can't reach the private one, so construct directly.
    return std::unique_ptr<UvcCameraDevice>(
        new UvcCameraDevice(std::move(descriptor), ctx, dev, devh, *format, mode));
}

UvcCameraDevice::UvcCameraDevice(CameraDescriptor descriptor, uvc_context_t* ctx, uvc_device_t* dev,
                                   uvc_device_handle_t* devh, RawFormatMatch format, StreamMode mode)
    : ctx_(ctx), dev_(dev), devh_(devh), descriptor_(std::move(descriptor)), format_(format), mode_(mode) {
    disableAutoModes();
    probeCapabilities();
}

UvcCameraDevice::~UvcCameraDevice() {
    if (streaming_) {
        uvc_stop_streaming(devh_);
    }
    uvc_close(devh_);
    uvc_unref_device(dev_);
    uvc_exit(ctx_);
}

void UvcCameraDevice::disableAutoModes() {
    constexpr std::uint8_t kAeModeManual = 1;
    uvc_set_ae_mode(devh_, kAeModeManual);
    uvc_set_white_balance_component_auto(devh_, 0);
}

void UvcCameraDevice::probeCapabilities() {
    if (std::uint32_t cur = 0, min = 0, max = 0;
        uvc_get_exposure_abs(devh_, &cur, UVC_GET_CUR) == UVC_SUCCESS &&
        uvc_get_exposure_abs(devh_, &min, UVC_GET_MIN) == UVC_SUCCESS &&
        uvc_get_exposure_abs(devh_, &max, UVC_GET_MAX) == UVC_SUCCESS) {
        std::uint32_t res = 1;
        uvc_get_exposure_abs(devh_, &res, UVC_GET_RES);
        CameraCapability cap;
        cap.id = ControlId::Exposure;
        cap.valueType = ControlValueType::Float;
        cap.supported = true;
        cap.minValue = min * kExposureTickMicroseconds;
        cap.maxValue = max * kExposureTickMicroseconds;
        cap.step = std::max<std::uint32_t>(res, 1) * kExposureTickMicroseconds;
        cap.defaultValue = cur * kExposureTickMicroseconds;
        cap.mutableWhileStreaming = true;
        cap.unit = "us";
        exposureCap_ = cap;
    }

    if (std::uint16_t cur = 0, min = 0, max = 0;
        uvc_get_gain(devh_, &cur, UVC_GET_CUR) == UVC_SUCCESS &&
        uvc_get_gain(devh_, &min, UVC_GET_MIN) == UVC_SUCCESS &&
        uvc_get_gain(devh_, &max, UVC_GET_MAX) == UVC_SUCCESS) {
        std::uint16_t res = 1;
        uvc_get_gain(devh_, &res, UVC_GET_RES);
        CameraCapability cap;
        cap.id = ControlId::Gain;
        cap.valueType = ControlValueType::Integer;
        cap.supported = true;
        cap.minValue = min;
        cap.maxValue = max;
        cap.step = std::max<std::uint16_t>(res, 1);
        cap.defaultValue = cur;
        cap.mutableWhileStreaming = true;
        gainCap_ = cap;
    }

    if (std::uint16_t cur = 0, min = 0, max = 0;
        uvc_get_gamma(devh_, &cur, UVC_GET_CUR) == UVC_SUCCESS &&
        uvc_get_gamma(devh_, &min, UVC_GET_MIN) == UVC_SUCCESS &&
        uvc_get_gamma(devh_, &max, UVC_GET_MAX) == UVC_SUCCESS) {
        std::uint16_t res = 1;
        uvc_get_gamma(devh_, &res, UVC_GET_RES);
        CameraCapability cap;
        cap.id = ControlId::Gamma;
        cap.valueType = ControlValueType::Float;
        cap.supported = true;
        cap.minValue = min;
        cap.maxValue = max;
        cap.step = std::max<std::uint16_t>(res, 1);
        cap.defaultValue = cur;
        cap.mutableWhileStreaming = true;
        gammaCap_ = cap;
    }

    if (std::int16_t cur = 0, min = 0, max = 0;
        uvc_get_brightness(devh_, &cur, UVC_GET_CUR) == UVC_SUCCESS &&
        uvc_get_brightness(devh_, &min, UVC_GET_MIN) == UVC_SUCCESS &&
        uvc_get_brightness(devh_, &max, UVC_GET_MAX) == UVC_SUCCESS) {
        std::int16_t res = 1;
        uvc_get_brightness(devh_, &res, UVC_GET_RES);
        CameraCapability cap;
        cap.id = ControlId::Brightness;
        cap.valueType = ControlValueType::Integer;
        cap.supported = true;
        cap.minValue = min;
        cap.maxValue = max;
        cap.step = std::max<std::int16_t>(res, 1);
        cap.defaultValue = cur;
        cap.mutableWhileStreaming = true;
        brightnessCap_ = cap;
    }

    if (std::uint16_t curBlue = 0, curRed = 0, minBlue = 0, minRed = 0, maxBlue = 0, maxRed = 0;
        uvc_get_white_balance_component(devh_, &curBlue, &curRed, UVC_GET_CUR) == UVC_SUCCESS &&
        uvc_get_white_balance_component(devh_, &minBlue, &minRed, UVC_GET_MIN) == UVC_SUCCESS &&
        uvc_get_white_balance_component(devh_, &maxBlue, &maxRed, UVC_GET_MAX) == UVC_SUCCESS) {
        // Blue/red share one min/max/step range on every UVC device seen —
        // the descriptor doesn't expose separate ranges per channel.
        CameraCapability cap;
        cap.valueType = ControlValueType::Integer;
        cap.supported = true;
        cap.minValue = std::min(minBlue, minRed);
        cap.maxValue = std::max(maxBlue, maxRed);
        cap.step = 1;
        cap.mutableWhileStreaming = true;
        cap.defaultValue = curRed;
        whiteBalanceCap_ = cap;
    }
}

CameraDescriptor UvcCameraDevice::descriptor() const { return descriptor_; }

std::vector<StreamMode> UvcCameraDevice::streamModes() const { return {mode_}; }

std::vector<CameraCapability> UvcCameraDevice::capabilities() const {
    std::vector<CameraCapability> caps;
    if (exposureCap_) caps.push_back(*exposureCap_);
    if (gainCap_) caps.push_back(*gainCap_);
    if (gammaCap_) caps.push_back(*gammaCap_);
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

Result<void> UvcCameraDevice::configure(const StreamConfiguration& configuration) {
    if (streaming_) {
        return Result<void>::failure(
            {ErrorCode::InternalInvariantViolation, "UvcCameraDevice cannot be reconfigured while streaming", ""});
    }
    if (configuration.mode.width != mode_.width || configuration.mode.height != mode_.height ||
        configuration.mode.storage != mode_.storage) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "UvcCameraDevice only supports its single advertised stream mode", ""});
    }
    const double targetFps = configuration.targetFps > 0 ? configuration.targetFps : mode_.maxFps;
    if (targetFps < mode_.minFps || targetFps > mode_.maxFps) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "targetFps is outside the supported mode's fps range", ""});
    }

    const auto err = uvc_get_stream_ctrl_format_size(devh_, &streamCtrl_, format_.libuvcFormat,
                                                       static_cast<int>(mode_.width), static_cast<int>(mode_.height),
                                                       static_cast<int>(std::lround(targetFps)));
    if (err != UVC_SUCCESS) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "Camera rejected the requested stream format", uvc_strerror(err)});
    }

    configured_ = true;
    return Result<void>::success();
}

Result<void> UvcCameraDevice::start(FrameSink& sink) {
    if (streaming_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "UvcCameraDevice is already streaming", ""});
    }
    if (!configured_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "configure() must succeed before start()", ""});
    }

    sink_ = &sink;
    sequence_ = 0;
    const auto err = uvc_start_streaming(devh_, &streamCtrl_, &UvcCameraDevice::frameCallback, this, 0);
    if (err != UVC_SUCCESS) {
        sink_ = nullptr;
        return Result<void>::failure(
            {ErrorCode::StreamStartFailed, "Failed to start UVC streaming", uvc_strerror(err)});
    }

    streaming_ = true;
    return Result<void>::success();
}

Result<void> UvcCameraDevice::stop() {
    if (!streaming_) {
        return Result<void>::success();
    }
    uvc_stop_streaming(devh_);
    streaming_ = false;
    sink_ = nullptr;
    return Result<void>::success();
}

Result<ControlValue> UvcCameraDevice::getControl(ControlId id) const {
    switch (id) {
        case ControlId::Exposure: {
            std::uint32_t value = 0;
            if (uvc_get_exposure_abs(devh_, &value, UVC_GET_CUR) != UVC_SUCCESS) {
                return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Failed to read exposure", ""});
            }
            return Result<ControlValue>::success(value * kExposureTickMicroseconds);
        }
        case ControlId::Gain: {
            std::uint16_t value = 0;
            if (uvc_get_gain(devh_, &value, UVC_GET_CUR) != UVC_SUCCESS) {
                return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Failed to read gain", ""});
            }
            return Result<ControlValue>::success(static_cast<std::int64_t>(value));
        }
        case ControlId::Gamma: {
            std::uint16_t value = 0;
            if (uvc_get_gamma(devh_, &value, UVC_GET_CUR) != UVC_SUCCESS) {
                return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Failed to read gamma", ""});
            }
            return Result<ControlValue>::success(static_cast<double>(value));
        }
        case ControlId::Brightness: {
            std::int16_t value = 0;
            if (uvc_get_brightness(devh_, &value, UVC_GET_CUR) != UVC_SUCCESS) {
                return Result<ControlValue>::failure(
                    {ErrorCode::UnsupportedControl, "Failed to read brightness", ""});
            }
            return Result<ControlValue>::success(static_cast<std::int64_t>(value));
        }
        case ControlId::WhiteBalanceRed:
        case ControlId::WhiteBalanceBlue: {
            std::uint16_t blue = 0, red = 0;
            if (uvc_get_white_balance_component(devh_, &blue, &red, UVC_GET_CUR) != UVC_SUCCESS) {
                return Result<ControlValue>::failure(
                    {ErrorCode::UnsupportedControl, "Failed to read white balance", ""});
            }
            return Result<ControlValue>::success(
                static_cast<std::int64_t>(id == ControlId::WhiteBalanceRed ? red : blue));
        }
        default:
            return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
    }
}

Result<void> UvcCameraDevice::setControl(ControlId id, const ControlValue& value) {
    if (id == ControlId::Exposure) {
        const double* asDouble = std::get_if<double>(&value);
        if (asDouble == nullptr || !exposureCap_ || *asDouble < exposureCap_->minValue ||
            *asDouble > exposureCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Exposure expects a double within [min, max] microseconds", ""});
        }
        const auto ticks = static_cast<std::uint32_t>(std::lround(*asDouble / kExposureTickMicroseconds));
        const auto err = uvc_set_exposure_abs(devh_, ticks);
        if (err != UVC_SUCCESS) {
            return Result<void>::failure({ErrorCode::UnsupportedControl, "Camera rejected exposure", uvc_strerror(err)});
        }
        return Result<void>::success();
    }
    if (id == ControlId::Gain) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || !gainCap_ || *asInt < gainCap_->minValue || *asInt > gainCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Gain expects an integer within [min, max]", ""});
        }
        const auto err = uvc_set_gain(devh_, static_cast<std::uint16_t>(*asInt));
        if (err != UVC_SUCCESS) {
            return Result<void>::failure({ErrorCode::UnsupportedControl, "Camera rejected gain", uvc_strerror(err)});
        }
        return Result<void>::success();
    }
    if (id == ControlId::Gamma) {
        const double* asDouble = std::get_if<double>(&value);
        if (asDouble == nullptr || !gammaCap_ || *asDouble < gammaCap_->minValue || *asDouble > gammaCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Gamma expects a double within [min, max]", ""});
        }
        const auto err = uvc_set_gamma(devh_, static_cast<std::uint16_t>(std::lround(*asDouble)));
        if (err != UVC_SUCCESS) {
            return Result<void>::failure({ErrorCode::UnsupportedControl, "Camera rejected gamma", uvc_strerror(err)});
        }
        return Result<void>::success();
    }
    if (id == ControlId::Brightness) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || !brightnessCap_ || *asInt < brightnessCap_->minValue ||
            *asInt > brightnessCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Brightness expects an integer within [min, max]", ""});
        }
        const auto err = uvc_set_brightness(devh_, static_cast<std::int16_t>(*asInt));
        if (err != UVC_SUCCESS) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Camera rejected brightness", uvc_strerror(err)});
        }
        return Result<void>::success();
    }
    if (id == ControlId::WhiteBalanceRed || id == ControlId::WhiteBalanceBlue) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || !whiteBalanceCap_ || *asInt < whiteBalanceCap_->minValue ||
            *asInt > whiteBalanceCap_->maxValue) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "White balance expects an integer within [min, max]", ""});
        }
        std::uint16_t blue = 0, red = 0;
        if (uvc_get_white_balance_component(devh_, &blue, &red, UVC_GET_CUR) != UVC_SUCCESS) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Failed to read current white balance before writing", ""});
        }
        (id == ControlId::WhiteBalanceRed ? red : blue) = static_cast<std::uint16_t>(*asInt);
        const auto err = uvc_set_white_balance_component(devh_, blue, red);
        if (err != UVC_SUCCESS) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Camera rejected white balance", uvc_strerror(err)});
        }
        return Result<void>::success();
    }
    return Result<void>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
}

void UvcCameraDevice::frameCallback(uvc_frame_t* frame, void* userPtr) {
    if (frame == nullptr || userPtr == nullptr) {
        return;
    }
    static_cast<UvcCameraDevice*>(userPtr)->onLibuvcFrame(*frame);
}

void UvcCameraDevice::onLibuvcFrame(const uvc_frame_t& frame) {
    if (sink_ == nullptr) {
        return;
    }

    // libuvc reuses the same internal buffer across callbacks (flags=0 in
    // uvc_start_streaming), so it must be copied before returning — same
    // requirement MockCameraDevice's testPattern_ satisfies by owning its
    // bytes outright.
    std::vector<std::byte> bytes(frame.data_bytes);
    std::memcpy(bytes.data(), frame.data, frame.data_bytes);

    FrameBuffer buffer;
    buffer.descriptor.width = frame.width;
    buffer.descriptor.height = frame.height;
    buffer.descriptor.strideBytes = static_cast<std::uint32_t>(frame.step);
    buffer.descriptor.storage = mode_.storage;
    buffer.descriptor.bayer = mode_.bayer;
    buffer.descriptor.validBits = mode_.validBits;
    buffer.metadata.sequence = sequence_.fetch_add(1, std::memory_order_relaxed);
    buffer.metadata.hostArrival = std::chrono::steady_clock::now();
    buffer.storage = std::make_shared<const BufferStorage>(std::move(bytes));

    sink_->onFrame(buffer);
}

}  // namespace astro::driver::uvc
