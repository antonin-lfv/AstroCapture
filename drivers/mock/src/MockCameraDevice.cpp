#include "MockCameraDevice.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>

#include "astro/driver/mock/BayerTestPattern.hpp"

namespace astro::driver::mock {

using namespace astro::domain;

namespace {

constexpr double kExposureMinUs = 100.0;
constexpr double kExposureMaxUs = 1'000'000.0;
constexpr double kExposureStepUs = 100.0;
constexpr double kExposureDefaultUs = 10'000.0;

constexpr std::int64_t kGainMin = 0;
constexpr std::int64_t kGainMax = 100;
constexpr std::int64_t kGainStep = 1;
constexpr std::int64_t kGainDefault = 0;

constexpr double kGammaMin = 0.1;
constexpr double kGammaMax = 3.0;
constexpr double kGammaStep = 0.05;
constexpr double kGammaDefault = 1.0;

constexpr std::int64_t kWhiteBalanceMin = 0;
constexpr std::int64_t kWhiteBalanceMax = 100;
constexpr std::int64_t kWhiteBalanceStep = 1;
constexpr std::int64_t kWhiteBalanceDefault = 50;

constexpr std::int64_t kBrightnessMin = 0;
constexpr std::int64_t kBrightnessMax = 255;
constexpr std::int64_t kBrightnessStep = 1;
constexpr std::int64_t kBrightnessDefault = 8;

StreamMode defaultSupportedMode() {
    StreamMode mode;
    mode.width = 640;
    mode.height = 480;
    mode.storage = PixelStorage::Bayer8;
    mode.bayer = BayerPattern::RGGB;
    mode.validBits = 8;
    mode.minFps = 1.0;
    mode.maxFps = 120.0;
    mode.roiSupported = true;
    return mode;
}

// Bayer's 2x2 channel tiling repeats every 2 pixels, so an ROI whose origin
// isn't on an even pixel would shift which channel lands where — reject
// rather than silently mis-color the crop.
bool isValidRoi(const Roi& roi, const StreamMode& mode) {
    if (roi.width == 0 && roi.height == 0) {
        return true;
    }
    return roi.width >= 2 && roi.height >= 2 && roi.width % 2 == 0 && roi.height % 2 == 0 && roi.x % 2 == 0 &&
           roi.y % 2 == 0 && roi.x + roi.width <= mode.width && roi.y + roi.height <= mode.height;
}

bool sameMode(const StreamMode& a, const StreamMode& b) {
    return a.width == b.width && a.height == b.height && a.storage == b.storage && a.bayer == b.bayer;
}

}  // namespace

MockCameraDevice::MockCameraDevice(CameraDescriptor descriptor)
    : descriptor_(std::move(descriptor)),
      supportedMode_(defaultSupportedMode()),
      frameRateLimit_(supportedMode_.maxFps) {}

MockCameraDevice::~MockCameraDevice() {
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
}

CameraDescriptor MockCameraDevice::descriptor() const { return descriptor_; }

std::vector<StreamMode> MockCameraDevice::streamModes() const { return {supportedMode_}; }

std::vector<CameraCapability> MockCameraDevice::capabilities() const {
    CameraCapability exposure;
    exposure.id = ControlId::Exposure;
    exposure.valueType = ControlValueType::Float;
    exposure.supported = true;
    exposure.minValue = kExposureMinUs;
    exposure.maxValue = kExposureMaxUs;
    exposure.step = kExposureStepUs;
    exposure.defaultValue = kExposureDefaultUs;
    exposure.mutableWhileStreaming = true;
    exposure.unit = "us";

    CameraCapability gain;
    gain.id = ControlId::Gain;
    gain.valueType = ControlValueType::Integer;
    gain.supported = true;
    gain.minValue = static_cast<double>(kGainMin);
    gain.maxValue = static_cast<double>(kGainMax);
    gain.step = static_cast<double>(kGainStep);
    gain.defaultValue = static_cast<double>(kGainDefault);
    gain.mutableWhileStreaming = true;

    CameraCapability gamma;
    gamma.id = ControlId::Gamma;
    gamma.valueType = ControlValueType::Float;
    gamma.supported = true;
    gamma.minValue = kGammaMin;
    gamma.maxValue = kGammaMax;
    gamma.step = kGammaStep;
    gamma.defaultValue = kGammaDefault;
    gamma.mutableWhileStreaming = true;

    CameraCapability whiteBalanceRed;
    whiteBalanceRed.id = ControlId::WhiteBalanceRed;
    whiteBalanceRed.valueType = ControlValueType::Integer;
    whiteBalanceRed.supported = true;
    whiteBalanceRed.minValue = static_cast<double>(kWhiteBalanceMin);
    whiteBalanceRed.maxValue = static_cast<double>(kWhiteBalanceMax);
    whiteBalanceRed.step = static_cast<double>(kWhiteBalanceStep);
    whiteBalanceRed.defaultValue = static_cast<double>(kWhiteBalanceDefault);
    whiteBalanceRed.mutableWhileStreaming = true;

    CameraCapability whiteBalanceBlue;
    whiteBalanceBlue.id = ControlId::WhiteBalanceBlue;
    whiteBalanceBlue.valueType = ControlValueType::Integer;
    whiteBalanceBlue.supported = true;
    whiteBalanceBlue.minValue = static_cast<double>(kWhiteBalanceMin);
    whiteBalanceBlue.maxValue = static_cast<double>(kWhiteBalanceMax);
    whiteBalanceBlue.step = static_cast<double>(kWhiteBalanceStep);
    whiteBalanceBlue.defaultValue = static_cast<double>(kWhiteBalanceDefault);
    whiteBalanceBlue.mutableWhileStreaming = true;

    CameraCapability brightness;
    brightness.id = ControlId::Brightness;
    brightness.valueType = ControlValueType::Integer;
    brightness.supported = true;
    brightness.minValue = static_cast<double>(kBrightnessMin);
    brightness.maxValue = static_cast<double>(kBrightnessMax);
    brightness.step = static_cast<double>(kBrightnessStep);
    brightness.defaultValue = static_cast<double>(kBrightnessDefault);
    brightness.mutableWhileStreaming = true;

    CameraCapability frameRateLimit;
    frameRateLimit.id = ControlId::FrameRateLimit;
    frameRateLimit.valueType = ControlValueType::Float;
    frameRateLimit.supported = true;
    frameRateLimit.minValue = supportedMode_.minFps;
    frameRateLimit.maxValue = supportedMode_.maxFps;
    frameRateLimit.step = 1.0;
    frameRateLimit.defaultValue = supportedMode_.maxFps;
    frameRateLimit.mutableWhileStreaming = true;
    frameRateLimit.unit = "fps";

    return {exposure, gain, gamma, whiteBalanceRed, whiteBalanceBlue, brightness, frameRateLimit};
}

Result<void> MockCameraDevice::configure(const StreamConfiguration& configuration) {
    if (running_) {
        return Result<void>::failure(
            {ErrorCode::InternalInvariantViolation, "MockCameraDevice cannot be reconfigured while streaming", ""});
    }
    if (!sameMode(configuration.mode, supportedMode_)) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "MockCameraDevice only supports its single advertised stream mode", ""});
    }
    if (configuration.targetFps < supportedMode_.minFps || configuration.targetFps > supportedMode_.maxFps) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "targetFps is outside the supported mode's fps range", ""});
    }
    if (configuration.roi.width > 0 && !supportedMode_.roiSupported) {
        return Result<void>::failure({ErrorCode::UnsupportedFormat, "This mode does not support an ROI", ""});
    }
    if (!isValidRoi(configuration.roi, supportedMode_)) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat,
             "ROI must have an even, in-bounds origin and even, non-zero dimensions", ""});
    }

    configuration_ = configuration;
    configured_ = true;
    return Result<void>::success();
}

Result<void> MockCameraDevice::start(FrameSink& sink) {
    if (running_) {
        return Result<void>::failure({ErrorCode::StreamStartFailed, "MockCameraDevice is already streaming", ""});
    }
    if (!configured_) {
        return Result<void>::failure(
            {ErrorCode::StreamStartFailed, "configure() must succeed before start()", ""});
    }

    if (!testPattern_) {
        const auto rgb = renderColorBarsRgb(supportedMode_.width, supportedMode_.height);
        const auto mosaic = mosaicToBayer8(rgb, supportedMode_.width, supportedMode_.height, supportedMode_.bayer);

        std::vector<std::byte> bytes(mosaic.size());
        std::transform(mosaic.begin(), mosaic.end(), bytes.begin(),
                        [](std::uint8_t value) { return std::byte{value}; });
        testPattern_ = std::make_shared<const BufferStorage>(std::move(bytes));
    }

    sink_ = &sink;
    sequence_ = 0;
    running_ = true;
    worker_ = std::thread(&MockCameraDevice::runLoop, this);
    return Result<void>::success();
}

Result<void> MockCameraDevice::stop() {
    if (!running_) {
        return Result<void>::success();
    }
    running_ = false;
    if (worker_.joinable()) {
        worker_.join();
    }
    sink_ = nullptr;
    return Result<void>::success();
}

Result<ControlValue> MockCameraDevice::getControl(ControlId id) const {
    std::lock_guard lock(controlsMutex_);
    switch (id) {
        case ControlId::Exposure:
            return Result<ControlValue>::success(exposureMicroseconds_);
        case ControlId::Gain:
            return Result<ControlValue>::success(gain_);
        case ControlId::Gamma:
            return Result<ControlValue>::success(gamma_);
        case ControlId::WhiteBalanceRed:
            return Result<ControlValue>::success(whiteBalanceRed_);
        case ControlId::WhiteBalanceBlue:
            return Result<ControlValue>::success(whiteBalanceBlue_);
        case ControlId::Brightness:
            return Result<ControlValue>::success(brightness_);
        case ControlId::FrameRateLimit:
            return Result<ControlValue>::success(frameRateLimit_);
        default:
            return Result<ControlValue>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
    }
}

Result<void> MockCameraDevice::setControl(ControlId id, const ControlValue& value) {
    std::lock_guard lock(controlsMutex_);
    if (id == ControlId::Exposure) {
        const double* asDouble = std::get_if<double>(&value);
        if (asDouble == nullptr || *asDouble < kExposureMinUs || *asDouble > kExposureMaxUs) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Exposure expects a double within [min, max] microseconds", ""});
        }
        exposureMicroseconds_ = *asDouble;
        return Result<void>::success();
    }
    if (id == ControlId::Gain) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || *asInt < kGainMin || *asInt > kGainMax) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Gain expects an integer within [min, max]", ""});
        }
        gain_ = *asInt;
        return Result<void>::success();
    }
    if (id == ControlId::Gamma) {
        const double* asDouble = std::get_if<double>(&value);
        if (asDouble == nullptr || *asDouble < kGammaMin || *asDouble > kGammaMax) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Gamma expects a double within [min, max]", ""});
        }
        gamma_ = *asDouble;
        return Result<void>::success();
    }
    if (id == ControlId::WhiteBalanceRed || id == ControlId::WhiteBalanceBlue) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || *asInt < kWhiteBalanceMin || *asInt > kWhiteBalanceMax) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "White balance expects an integer within [min, max]", ""});
        }
        (id == ControlId::WhiteBalanceRed ? whiteBalanceRed_ : whiteBalanceBlue_) = *asInt;
        return Result<void>::success();
    }
    if (id == ControlId::Brightness) {
        const std::int64_t* asInt = std::get_if<std::int64_t>(&value);
        if (asInt == nullptr || *asInt < kBrightnessMin || *asInt > kBrightnessMax) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "Brightness expects an integer within [min, max]", ""});
        }
        brightness_ = *asInt;
        return Result<void>::success();
    }
    if (id == ControlId::FrameRateLimit) {
        const double* asDouble = std::get_if<double>(&value);
        if (asDouble == nullptr || *asDouble < supportedMode_.minFps || *asDouble > supportedMode_.maxFps) {
            return Result<void>::failure(
                {ErrorCode::UnsupportedControl, "FrameRateLimit expects a double within the mode's fps range", ""});
        }
        frameRateLimit_ = *asDouble;
        return Result<void>::success();
    }
    return Result<void>::failure({ErrorCode::UnsupportedControl, "Control not supported", ""});
}

std::shared_ptr<const BufferStorage> MockCameraDevice::cropToRoi(const Roi& roi) const {
    const auto* fullBytes = reinterpret_cast<const std::uint8_t*>(testPattern_->data());
    std::vector<std::byte> cropped(static_cast<std::size_t>(roi.width) * roi.height);
    for (std::uint32_t row = 0; row < roi.height; ++row) {
        const std::size_t srcOffset = (static_cast<std::size_t>(roi.y + row) * supportedMode_.width) + roi.x;
        const std::size_t dstOffset = static_cast<std::size_t>(row) * roi.width;
        std::memcpy(cropped.data() + dstOffset, fullBytes + srcOffset, roi.width);
    }
    return std::make_shared<const BufferStorage>(std::move(cropped));
}

void MockCameraDevice::runLoop() {
    while (running_.load(std::memory_order_relaxed)) {
        const Roi& roi = configuration_.roi;
        const bool cropped = roi.width > 0;

        FrameBuffer frame;
        frame.descriptor.width = cropped ? roi.width : supportedMode_.width;
        frame.descriptor.height = cropped ? roi.height : supportedMode_.height;
        frame.descriptor.storage = supportedMode_.storage;
        frame.descriptor.bayer = supportedMode_.bayer;
        frame.descriptor.validBits = supportedMode_.validBits;
        frame.metadata.sequence = sequence_.fetch_add(1, std::memory_order_relaxed);
        frame.metadata.hostArrival = std::chrono::steady_clock::now();
        frame.storage = cropped ? cropToRoi(roi) : testPattern_;

        sink_->onFrame(frame);

        double fps;
        {
            std::lock_guard lock(controlsMutex_);
            fps = frameRateLimit_ > 0.0 ? frameRateLimit_ : supportedMode_.maxFps;
        }
        std::this_thread::sleep_for(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::duration<double>(1.0 / fps)));
    }
}

}  // namespace astro::driver::mock
