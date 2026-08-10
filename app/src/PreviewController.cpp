#include "PreviewController.hpp"

#include "SimpleDebayer.hpp"
#include "astro/domain/StreamMode.hpp"

#ifdef ASTRO_ENABLE_MOCK_CAMERA
#include "astro/driver/mock/MockCameraBackend.hpp"
#endif

namespace astro::app {

using namespace astro::domain;

PreviewController::PreviewController(QObject* parent) : QObject(parent) {}

PreviewController::~PreviewController() {
    if (device_) {
        device_->stop();
    }
}

void PreviewController::rescan() {
    if (connected_) {
        return;
    }

#ifdef ASTRO_ENABLE_MOCK_CAMERA
    astro::driver::mock::MockCameraBackend backend;
    const auto descriptors = backend.enumerate();
    if (descriptors.empty()) {
        errorMessage_ = QStringLiteral("No camera found.");
        emit errorMessageChanged();
        return;
    }

    auto device = backend.open(descriptors.front().id);
    if (!device) {
        errorMessage_ = QStringLiteral("Failed to open camera.");
        emit errorMessageChanged();
        return;
    }

    const auto modes = device->streamModes();
    if (modes.empty()) {
        errorMessage_ = QStringLiteral("Camera exposes no stream modes.");
        emit errorMessageChanged();
        return;
    }

    StreamConfiguration configuration;
    configuration.mode = modes.front();
    configuration.targetFps = modes.front().maxFps;

    if (const auto result = device->configure(configuration); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
        emit errorMessageChanged();
        return;
    }

    if (const auto result = device->start(*this); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
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
    connected_ = true;
    emit connectedChanged();
    emit errorMessageChanged();
    emit frameCountChanged();
#else
    errorMessage_ = QStringLiteral("No camera backends compiled in.");
    emit errorMessageChanged();
#endif
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
    return debayerToPreviewImage(latest->descriptor, latest->storage->data(), latest->storage->size());
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
