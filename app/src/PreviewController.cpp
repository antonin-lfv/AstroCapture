#include "PreviewController.hpp"

#include "FramePreviewImage.hpp"
#include "astro/domain/StreamMode.hpp"

#ifdef ASTRO_ENABLE_MOCK_CAMERA
#include "astro/driver/mock/MockCameraBackend.hpp"
#endif
#ifdef ASTRO_ENABLE_DRIVER_UVC
#include "astro/driver/uvc/UvcCameraBackend.hpp"
#endif

namespace astro::app {

using namespace astro::domain;

PreviewController::PreviewController(QObject* parent) : QObject(parent) {}

PreviewController::~PreviewController() {
    if (device_) {
        device_->stop();
    }
}

std::unique_ptr<ICameraDevice> PreviewController::tryOpenFirstDevice(ICameraBackend& backend) {
    const auto descriptors = backend.enumerate();
    if (descriptors.empty()) {
        return nullptr;
    }

    auto device = backend.open(descriptors.front().id);
    if (!device) {
        errorMessage_ = QStringLiteral("Failed to open camera.");
        return nullptr;
    }

    const auto modes = device->streamModes();
    if (modes.empty()) {
        errorMessage_ = QStringLiteral("Camera exposes no stream modes.");
        return nullptr;
    }

    StreamConfiguration configuration;
    configuration.mode = modes.front();
    configuration.targetFps = modes.front().maxFps;

    if (const auto result = device->configure(configuration); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
        return nullptr;
    }

    if (const auto result = device->start(*this); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
        return nullptr;
    }

    return device;
}

void PreviewController::rescan() {
    if (connected_) {
        return;
    }

    errorMessage_.clear();
    std::unique_ptr<ICameraDevice> device;

    // Real hardware first: if a UVC/webcam is attached, prefer it over the
    // synthetic mock so "Rescan" shows something real whenever possible.
#ifdef ASTRO_ENABLE_DRIVER_UVC
    if (!device) {
        astro::driver::uvc::UvcCameraBackend uvcBackend;
        device = tryOpenFirstDevice(uvcBackend);
    }
#endif
#ifdef ASTRO_ENABLE_MOCK_CAMERA
    if (!device) {
        astro::driver::mock::MockCameraBackend mockBackend;
        device = tryOpenFirstDevice(mockBackend);
    }
#endif

    if (!device) {
        if (errorMessage_.isEmpty()) {
            errorMessage_ = QStringLiteral("No camera found.");
        }
        emit errorMessageChanged();
        return;
    }

    device_ = std::move(device);
    cameraName_ = QString::fromStdString(device_->descriptor().displayName);
    errorMessage_.clear();
    frameCount_.store(0, std::memory_order_relaxed);
    renderedFrameCount_.store(0, std::memory_order_relaxed);
    lastRenderedSequence_.reset();
    startTime_ = std::chrono::steady_clock::now();

    exposureCapability_.reset();
    gainCapability_.reset();
    for (const auto& capability : device_->capabilities()) {
        if (capability.id == ControlId::Exposure && capability.supported) {
            exposureCapability_ = capability;
            if (const auto value = device_->getControl(ControlId::Exposure);
                value.ok()) {
                if (const auto* asDouble = std::get_if<double>(&value.value())) {
                    exposureMicroseconds_ = *asDouble;
                }
            }
        } else if (capability.id == ControlId::Gain && capability.supported) {
            gainCapability_ = capability;
            if (const auto value = device_->getControl(ControlId::Gain); value.ok()) {
                if (const auto* asInt = std::get_if<std::int64_t>(&value.value())) {
                    gainValue_ = *asInt;
                }
            }
        }
    }

    connected_ = true;
    emit connectedChanged();
    emit errorMessageChanged();
    emit frameCountChanged();
    emit exposureChanged();
    emit gainChanged();
}

void PreviewController::setExposureMicroseconds(double value) {
    if (!device_ || !exposureCapability_) {
        return;
    }
    if (const auto result = device_->setControl(ControlId::Exposure, value); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
        emit errorMessageChanged();
        return;
    }
    exposureMicroseconds_ = value;
    emit exposureChanged();
}

void PreviewController::setGainValue(int value) {
    if (!device_ || !gainCapability_) {
        return;
    }
    const auto asInt64 = static_cast<std::int64_t>(value);
    if (const auto result = device_->setControl(ControlId::Gain, asInt64); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
        emit errorMessageChanged();
        return;
    }
    gainValue_ = asInt64;
    emit gainChanged();
}

double PreviewController::fps() const {
    const auto acquired = frameCount_.load(std::memory_order_relaxed);
    if (!connected_ || acquired == 0) {
        return 0.0;
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
    return seconds > 0.0 ? static_cast<double>(acquired) / seconds : 0.0;
}

quint64 PreviewController::previewSkippedFrames() const {
    const auto acquired = frameCount_.load(std::memory_order_relaxed);
    const auto rendered = renderedFrameCount_.load(std::memory_order_relaxed);
    return acquired > rendered ? acquired - rendered : 0;
}

QImage PreviewController::frame() const {
    const auto latest = frameSlot_.latest();
    if (!latest) {
        return QImage();
    }
    if (lastRenderedSequence_ != latest->metadata.sequence) {
        lastRenderedSequence_ = latest->metadata.sequence;
        renderedFrameCount_.fetch_add(1, std::memory_order_relaxed);
    }
    return frameToPreviewImage(latest->descriptor, latest->storage->data(), latest->storage->size());
}

void PreviewController::onFrame(const FrameBuffer& frameBuffer) {
    frameSlot_.store(frameBuffer);
    frameCount_.fetch_add(1, std::memory_order_relaxed);

    // Signal emission is thread-safe in Qt: connections to GUI-thread
    // receivers (LivePreviewItem, QML property bindings) are queued
    // automatically, so this stays cheap on the acquisition thread.
    emit frameChanged();
    emit frameCountChanged();
}

}  // namespace astro::app
