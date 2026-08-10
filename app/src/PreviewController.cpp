#include "PreviewController.hpp"

#include <QMetaObject>

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
    frameCount_ = 0;
    connected_ = true;
    emit connectedChanged();
    emit errorMessageChanged();
    emit frameCountChanged();
#else
    errorMessage_ = QStringLiteral("No camera backends compiled in.");
    emit errorMessageChanged();
#endif
}

void PreviewController::onFrame(const FrameBuffer& frameBuffer) {
    QImage image = debayerToPreviewImage(frameBuffer.descriptor, frameBuffer.storage->data(),
                                          frameBuffer.storage->size());
    const quint64 sequence = frameBuffer.metadata.sequence;

    // onFrame runs on the camera's acquisition thread; hop to the GUI thread
    // (this object's thread) before touching state QML/paint code reads.
    QMetaObject::invokeMethod(
        this,
        [this, image = std::move(image), sequence]() {
            frame_ = image;
            frameCount_ = sequence + 1;
            emit frameChanged();
            emit frameCountChanged();
        },
        Qt::QueuedConnection);
}

}  // namespace astro::app
