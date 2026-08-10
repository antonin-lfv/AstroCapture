#include "PreviewController.hpp"

#include <algorithm>

#include <QDir>
#include <QStandardPaths>

#include "FileNaming.hpp"
#include "FramePreviewImage.hpp"
#include "astro/domain/StreamMode.hpp"
#include "astro/recording/RawSequenceRecorder.hpp"

#ifdef ASTRO_ENABLE_MOCK_CAMERA
#include "astro/driver/mock/MockCameraBackend.hpp"
#endif
#ifdef ASTRO_ENABLE_DRIVER_UVC
#include "astro/driver/uvc/UvcCameraBackend.hpp"
#endif

namespace astro::app {

using namespace astro::domain;

PreviewController::PreviewController(QObject* parent) : QObject(parent) {
    saveDirectory_ = settings_.loadRootDirectory().value_or(QString());
    // The writer thread can't call stopRecording() itself (it would try to
    // join its own thread), so it emits recordingTargetReached() instead;
    // Qt queues delivery to this object's thread (GUI) automatically.
    connect(this, &PreviewController::recordingTargetReached, this, &PreviewController::stopRecording,
            Qt::QueuedConnection);
}

PreviewController::~PreviewController() {
    if (recording_) {
        stopRecording();
    }
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

    // Restore any previously saved values for this specific camera,
    // overriding the device's own power-on defaults.
    const QString cameraIdString = QString::fromStdString(device_->descriptor().id);
    if (exposureCapability_) {
        if (const auto saved = settings_.loadExposureMicroseconds(cameraIdString); saved) {
            setExposureMicroseconds(
                std::clamp(*saved, exposureCapability_->minValue, exposureCapability_->maxValue));
        }
    }
    if (gainCapability_) {
        if (const auto saved = settings_.loadGain(cameraIdString); saved) {
            const auto clamped = std::clamp(*saved, static_cast<qint64>(gainCapability_->minValue),
                                             static_cast<qint64>(gainCapability_->maxValue));
            setGainValue(static_cast<int>(clamped));
        }
    }
}

void PreviewController::disconnectCamera() {
    if (!connected_) {
        return;
    }
    if (recording_) {
        stopRecording();
    }
    if (device_) {
        device_->stop();
        device_.reset();
    }
    frameSlot_.clear();
    exposureCapability_.reset();
    gainCapability_.reset();

    connected_ = false;
    cameraName_.clear();
    frameCount_.store(0, std::memory_order_relaxed);
    renderedFrameCount_.store(0, std::memory_order_relaxed);
    lastRenderedSequence_.reset();

    emit connectedChanged();
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
    settings_.saveExposureMicroseconds(QString::fromStdString(device_->descriptor().id), value);
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
    settings_.saveGain(QString::fromStdString(device_->descriptor().id), asInt64);
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

astro::processing::Histogram PreviewController::histogram() const {
    const auto latest = frameSlot_.latest();
    if (!latest) {
        return {};
    }
    return astro::processing::computeHistogram(latest->descriptor, latest->storage->data(),
                                                 latest->storage->size());
}

void PreviewController::captureStillImage() {
    if (!connected_ || !device_) {
        lastCaptureStatus_ = QStringLiteral("No camera connected.");
        emit lastCaptureStatusChanged();
        return;
    }

    const QImage image = frame();
    if (image.isNull()) {
        lastCaptureStatus_ = QStringLiteral("No frame available yet.");
        emit lastCaptureStatusChanged();
        return;
    }

    const QString directory = imagesDirectoryPath();
    if (!QDir().mkpath(directory)) {
        lastCaptureStatus_ = QStringLiteral("Failed to create ") + directory;
        emit lastCaptureStatusChanged();
        return;
    }

    const QString cameraId = QString::fromStdString(device_->descriptor().id);
    const QString path =
        directory + QLatin1Char('/') + timestampedBaseName(cameraId, targetName_) + QStringLiteral(".png");

    if (!image.save(path, "PNG")) {
        lastCaptureStatus_ = QStringLiteral("Failed to save image.");
        emit lastCaptureStatusChanged();
        return;
    }

    lastCaptureStatus_ = QStringLiteral("Saved ") + path;
    emit lastCaptureStatusChanged();
}

double PreviewController::recordingElapsedSeconds() const {
    if (!recording_) {
        return 0.0;
    }
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - recordingStartTime_).count();
}

void PreviewController::setRecordingTargetFrameCount(int value) {
    if (recording_ || recordingTargetFrameCount_ == value) {
        return;
    }
    recordingTargetFrameCount_ = std::max(0, value);
    emit recordingTargetChanged();
}

void PreviewController::setRecordingTargetDurationSeconds(double value) {
    if (recording_ || recordingTargetDurationSeconds_ == value) {
        return;
    }
    recordingTargetDurationSeconds_ = std::max(0.0, value);
    emit recordingTargetChanged();
}

void PreviewController::startRecording() {
    if (!connected_ || !device_) {
        recordingStatus_ = QStringLiteral("No camera connected.");
        emit recordingStatusChanged();
        return;
    }
    if (recording_) {
        return;
    }

    const auto latest = frameSlot_.latest();
    if (!latest) {
        recordingStatus_ = QStringLiteral("No frame available yet.");
        emit recordingStatusChanged();
        return;
    }

    const QString directory = videosDirectoryPath();
    if (!QDir().mkpath(directory)) {
        recordingStatus_ = QStringLiteral("Failed to create ") + directory;
        emit recordingStatusChanged();
        return;
    }

    const QString cameraId = QString::fromStdString(device_->descriptor().id);
    const QString path =
        directory + QLatin1Char('/') + timestampedBaseName(cameraId, targetName_) + QStringLiteral(".acraw");

    auto recorder = std::make_unique<astro::recording::RawSequenceRecorder>();
    const double nominalFps = fps() > 0.0 ? fps() : 30.0;  // best-effort header hint; not used for pacing
    if (const auto result = recorder->open(path.toStdString(), latest->descriptor, nominalFps); !result.ok()) {
        recordingStatus_ = QString::fromStdString(result.error().message);
        emit recordingStatusChanged();
        return;
    }

    // Publish the queue under the lock so onFrame() (acquisition thread)
    // never observes a partially-constructed queue; capture a stable raw
    // pointer for the writer thread rather than letting it re-read
    // recordingQueue_ each iteration, since stopRecording() nulls that
    // member out while the thread may still be draining (see there).
    auto queue = std::make_unique<astro::domain::BoundedFrameQueue>(64);
    astro::domain::BoundedFrameQueue* queuePtr = queue.get();
    astro::recording::IRecorder* recorderPtr = recorder.get();
    {
        std::lock_guard lock(recordingMutex_);
        recordingQueue_ = std::move(queue);
    }
    recorder_ = std::move(recorder);

    recordingFrameCount_.store(0, std::memory_order_relaxed);
    recordingDroppedFrames_.store(0, std::memory_order_relaxed);
    recordingStartTime_ = std::chrono::steady_clock::now();
    recordingPath_ = path;
    recording_ = true;

    // Read once here (GUI thread, before the writer thread starts): thread
    // creation happens-before the thread's first instruction, so plain
    // values captured by the lambda are safely visible without atomics,
    // as long as nothing mutates them while recording_ is true (the
    // setters above already refuse to).
    const int targetFrameCount = recordingTargetFrameCount_;
    const double targetDurationSeconds = recordingTargetDurationSeconds_;
    const auto recordingStart = recordingStartTime_;

    recordingThread_ = std::thread([this, queuePtr, recorderPtr, targetFrameCount, targetDurationSeconds,
                                     recordingStart] {
        while (auto queuedFrame = queuePtr->waitPop()) {
            const auto result = recorderPtr->writeFrame(*queuedFrame);
            std::uint64_t written = 0;
            if (result.ok()) {
                written = recordingFrameCount_.fetch_add(1, std::memory_order_relaxed) + 1;
            } else {
                recordingDroppedFrames_.fetch_add(1, std::memory_order_relaxed);
            }
            emit recordingStatsChanged();

            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - recordingStart)
                                        .count();
            const bool frameTargetReached = targetFrameCount > 0 && written >= static_cast<std::uint64_t>(targetFrameCount);
            const bool durationTargetReached = targetDurationSeconds > 0.0 && elapsed >= targetDurationSeconds;
            if (frameTargetReached || durationTargetReached) {
                // Frames arriving after this point (before stopRecording()
                // actually runs on the GUI thread) queue up and, once the
                // bounded queue fills, count as dropped — an expected,
                // harmless artifact of hitting a target, not a real
                // integrity failure.
                emit recordingTargetReached();
                break;
            }
        }
    });

    recordingStatus_.clear();
    emit recordingChanged();
    emit recordingStatusChanged();
    emit recordingStatsChanged();
}

void PreviewController::stopRecording() {
    if (!recording_) {
        return;
    }

    // Order matters: stop() wakes the writer thread's waitPop() so it can
    // drain whatever's left and exit; join() guarantees it has actually
    // exited (and so is done touching queuePtr/recorderPtr) before we
    // destroy the objects those pointers reference below.
    recordingQueue_->stop();
    if (recordingThread_.joinable()) {
        recordingThread_.join();
    }

    {
        std::lock_guard lock(recordingMutex_);
        recordingQueue_.reset();
    }

    const auto summary = recorder_->close();
    if (summary.ok()) {
        recordingStatus_ = QStringLiteral("Saved ") + recordingPath_ + QStringLiteral(" (") +
                            QString::number(summary.value().framesWritten) + QStringLiteral(" frames, ") +
                            QString::number(recordingDroppedFrames_.load(std::memory_order_relaxed)) +
                            QStringLiteral(" dropped)");
    } else {
        recordingStatus_ = QString::fromStdString(summary.error().message);
    }
    recorder_.reset();

    recording_ = false;
    emit recordingChanged();
    emit recordingStatusChanged();
}

void PreviewController::onFrame(const FrameBuffer& frameBuffer) {
    frameSlot_.store(frameBuffer);
    frameCount_.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard lock(recordingMutex_);
        if (recordingQueue_ && !recordingQueue_->tryPush(frameBuffer)) {
            recordingDroppedFrames_.fetch_add(1, std::memory_order_relaxed);
            emit recordingStatsChanged();
        }
    }

    // Signal emission is thread-safe in Qt: connections to GUI-thread
    // receivers (LivePreviewItem, QML property bindings) are queued
    // automatically, so this stays cheap on the acquisition thread.
    emit frameChanged();
    emit frameCountChanged();
}

QString PreviewController::imagesDirectoryPath() const {
    if (!saveDirectory_.isEmpty()) {
        return saveDirectory_ + QStringLiteral("/Images");
    }
    return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + QStringLiteral("/AstroCapture");
}

QString PreviewController::videosDirectoryPath() const {
    if (!saveDirectory_.isEmpty()) {
        return saveDirectory_ + QStringLiteral("/Videos");
    }
    return QStandardPaths::writableLocation(QStandardPaths::MoviesLocation) + QStringLiteral("/AstroCapture");
}

void PreviewController::setSaveDirectory(const QString& path) {
    if (saveDirectory_ == path) {
        return;
    }
    saveDirectory_ = path;
    settings_.saveRootDirectory(path);
    emit saveDirectoryChanged();
}

void PreviewController::setTargetName(const QString& value) {
    if (targetName_ == value) {
        return;
    }
    targetName_ = value;
    emit targetNameChanged();
}

QStringList PreviewController::presetNames() const {
    if (!device_) {
        return {};
    }
    return settings_.presetNames(QString::fromStdString(device_->descriptor().id));
}

void PreviewController::savePreset(const QString& name) {
    if (!device_ || name.trimmed().isEmpty()) {
        return;
    }
    CameraPreset preset;
    preset.name = name.trimmed();
    preset.exposureMicroseconds = exposureMicroseconds_;
    preset.gain = gainValue_;
    settings_.savePreset(QString::fromStdString(device_->descriptor().id), preset);
    emit presetsChanged();
}

void PreviewController::loadPreset(const QString& name) {
    if (!device_) {
        return;
    }
    const auto preset = settings_.loadPreset(QString::fromStdString(device_->descriptor().id), name);
    if (!preset) {
        return;
    }
    if (exposureCapability_) {
        setExposureMicroseconds(
            std::clamp(preset->exposureMicroseconds, exposureCapability_->minValue, exposureCapability_->maxValue));
    }
    if (gainCapability_) {
        const auto clamped = std::clamp(preset->gain, static_cast<qint64>(gainCapability_->minValue),
                                         static_cast<qint64>(gainCapability_->maxValue));
        setGainValue(static_cast<int>(clamped));
    }
}

void PreviewController::deletePreset(const QString& name) {
    if (!device_) {
        return;
    }
    settings_.deletePreset(QString::fromStdString(device_->descriptor().id), name);
    emit presetsChanged();
}

}  // namespace astro::app
