#include "PreviewController.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <QDesktopServices>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

#include "FileNaming.hpp"
#include "FramePreviewImage.hpp"
#include "astro/domain/StreamMode.hpp"
#include "astro/recording/FitsWriter.hpp"
#include "astro/recording/RawSequenceRecorder.hpp"
#include "astro/recording/SerRecorder.hpp"
#include "astro/recording/TiffWriter.hpp"

#ifdef ASTRO_ENABLE_MOCK_CAMERA
#include "astro/driver/mock/MockCameraBackend.hpp"
#endif

namespace astro::app {

using namespace astro::domain;

namespace {

// Display metadata for the "extra controls" (Gate D backlog 22.3) that go
// through PreviewController::extraControls() instead of dedicated
// properties. Returns false for ids that don't belong in that list
// (Exposure/Gain have their own properties; everything else isn't a
// scalar control at all, e.g. Temperature/Cooler/Trigger aren't wired up
// yet).
bool extraControlMeta(ControlId id, QString& key, QString& label, QString& unit, bool& isFloat) {
    switch (id) {
        case ControlId::Gamma:
            key = QStringLiteral("gamma");
            label = QStringLiteral("Gamma");
            unit.clear();
            isFloat = true;
            return true;
        case ControlId::WhiteBalanceRed:
            key = QStringLiteral("whiteBalanceRed");
            label = QStringLiteral("White Balance (R)");
            unit.clear();
            isFloat = false;
            return true;
        case ControlId::WhiteBalanceBlue:
            key = QStringLiteral("whiteBalanceBlue");
            label = QStringLiteral("White Balance (B)");
            unit.clear();
            isFloat = false;
            return true;
        case ControlId::Brightness:
            key = QStringLiteral("brightness");
            label = QStringLiteral("Brightness");
            unit.clear();
            isFloat = false;
            return true;
        case ControlId::FrameRateLimit:
            key = QStringLiteral("frameRateLimit");
            label = QStringLiteral("Frame Rate Limit");
            unit = QStringLiteral("fps");
            isFloat = true;
            return true;
        default:
            return false;
    }
}

}  // namespace

PreviewController::PreviewController(QObject* parent) : QObject(parent) {
    saveDirectory_ = settings_.loadRootDirectory().value_or(QString());
    const QString loadedFormat = settings_.loadImageFormat().value_or(QStringLiteral("PNG"));
    saveFormat_ = (loadedFormat == QStringLiteral("TIFF16") || loadedFormat == QStringLiteral("FITS"))
                      ? loadedFormat
                      : QStringLiteral("PNG");
    const QString loadedVideoFormat = settings_.loadVideoFormat().value_or(QStringLiteral("SER"));
    videoFormat_ = loadedVideoFormat == QStringLiteral("ACRAW") ? loadedVideoFormat : QStringLiteral("SER");
    // The writer thread can't call stopRecording() itself (it would try to
    // join its own thread), so it emits recordingTargetReached() instead;
    // Qt queues delivery to this object's thread (GUI) automatically.
    connect(this, &PreviewController::recordingTargetReached, this, &PreviewController::stopRecording,
            Qt::QueuedConnection);

    // 20Hz: smooth enough for a live camera preview, far cheaper than
    // repainting (and re-running debayer/histogram) at the camera's own
    // fps — see previewTick()'s comment. Deliberately chosen rather than
    // relying on Qt's own render-loop pacing to cap the rate: measured
    // behavior of that varies (16fps on this dev machine, for reasons
    // unrelated to how expensive paint() actually is), and isn't something
    // this project controls or can guarantee across platforms. Only
    // running while connected (finishConnecting()/disconnectCamera()
    // start/stop it) so it costs nothing at idle.
    previewTimer_.setInterval(50);
    connect(&previewTimer_, &QTimer::timeout, this, &PreviewController::previewTick);
}

PreviewController::~PreviewController() {
    if (recording_) {
        stopRecording();
    }
    if (device_) {
        device_->stop();
    }
}

std::unique_ptr<ICameraDevice> PreviewController::openAndStart(ICameraBackend& backend, const CameraId& id) {
    auto device = backend.open(id);
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

    currentConfiguration_ = configuration;
    return device;
}

std::unique_ptr<ICameraDevice> PreviewController::openCandidate(const CameraCandidate& candidate) {
#ifdef ASTRO_ENABLE_MOCK_CAMERA
    if (candidate.backendId == QStringLiteral("mock")) {
        astro::driver::mock::MockCameraBackend backend;
        return openAndStart(backend, candidate.id.toStdString());
    }
#endif
    errorMessage_ = QStringLiteral("Camera backend not available in this build.");
    return nullptr;
}

void PreviewController::finishConnecting(std::unique_ptr<ICameraDevice> device) {
    device_ = std::move(device);
    cameraName_ = QString::fromStdString(device_->descriptor().displayName);
    errorMessage_.clear();
    frameCount_.store(0, std::memory_order_relaxed);
    renderedFrameCount_.store(0, std::memory_order_relaxed);
    lastRenderedSequence_.reset();
    cachedFrameImage_ = QImage();
    startTime_ = std::chrono::steady_clock::now();
    previewTimer_.start();

    roiSupported_ = currentConfiguration_.mode.roiSupported;
    sensorWidth_ = currentConfiguration_.mode.width;
    sensorHeight_ = currentConfiguration_.mode.height;

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
    emit roiChanged();

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

    loadExtraControls(cameraIdString);
}

void PreviewController::loadExtraControls(const QString& cameraIdString) {
    extraControls_.clear();
    for (const auto& capability : device_->capabilities()) {
        if (!capability.supported) {
            continue;
        }
        QString key;
        QString label;
        QString unit;
        bool isFloat = false;
        if (!extraControlMeta(capability.id, key, label, unit, isFloat)) {
            continue;
        }

        ExtraControl control;
        control.id = capability.id;
        control.key = key;
        control.label = label;
        control.unit = unit;
        control.isFloat = isFloat;
        control.minValue = capability.minValue;
        control.maxValue = capability.maxValue;
        control.step = capability.step > 0.0 ? capability.step : 1.0;
        control.value = capability.defaultValue;
        if (const auto current = device_->getControl(capability.id); current.ok()) {
            if (const auto* asDouble = std::get_if<double>(&current.value())) {
                control.value = *asDouble;
            } else if (const auto* asInt = std::get_if<std::int64_t>(&current.value())) {
                control.value = static_cast<double>(*asInt);
            }
        }

        if (const auto saved = settings_.loadExtraControl(cameraIdString, key); saved) {
            const double clamped = std::clamp(*saved, control.minValue, control.maxValue);
            const ControlValue controlValue =
                isFloat ? ControlValue{clamped} : ControlValue{static_cast<std::int64_t>(std::llround(clamped))};
            if (device_->setControl(capability.id, controlValue).ok()) {
                control.value = clamped;
            }
        }

        extraControls_.push_back(control);
    }
    emit extraControlsChanged();
}

void PreviewController::rescan() {
    if (connected_) {
        return;
    }

    errorMessage_.clear();

    // Real hardware only counts here: mock is a fallback, never a peer
    // choice, so it must not turn a single-camera rescan into a 2-item
    // picker (real + mock) — see the class comment. No real backend is
    // compiled in yet (Gate C), so this stays empty until one exists; a
    // future backend enumerates into realCandidates here the same way the
    // removed UVC backend did.
    std::vector<CameraCandidate> realCandidates;

    if (realCandidates.size() == 1) {
        if (auto device = openCandidate(realCandidates.front())) {
            finishConnecting(std::move(device));
        } else {
            emit errorMessageChanged();
        }
        return;
    }

    if (realCandidates.size() > 1) {
        availableCameras_ = std::move(realCandidates);
        emit availableCamerasChanged();
        return;
    }

#ifdef ASTRO_ENABLE_MOCK_CAMERA
    {
        astro::driver::mock::MockCameraBackend backend;
        const auto descriptors = backend.enumerate();
        if (!descriptors.empty()) {
            const CameraCandidate mockCandidate{QString::fromStdString(descriptors.front().id),
                                                 QString::fromStdString(descriptors.front().displayName),
                                                 QStringLiteral("mock")};
            if (auto device = openCandidate(mockCandidate)) {
                finishConnecting(std::move(device));
            } else {
                emit errorMessageChanged();
            }
            return;
        }
    }
#endif

    if (errorMessage_.isEmpty()) {
        errorMessage_ = QStringLiteral("No camera found.");
    }
    emit errorMessageChanged();
}

QVariantList PreviewController::availableCameras() const {
    QVariantList list;
    for (const auto& candidate : availableCameras_) {
        QVariantMap entry;
        entry[QStringLiteral("id")] = candidate.id;
        entry[QStringLiteral("displayName")] = candidate.displayName;
        list.append(entry);
    }
    return list;
}

void PreviewController::connectToCamera(const QString& cameraId) {
    if (connected_) {
        return;
    }
    for (const auto& candidate : availableCameras_) {
        if (candidate.id != cameraId) {
            continue;
        }
        errorMessage_.clear();
        if (auto device = openCandidate(candidate)) {
            availableCameras_.clear();
            emit availableCamerasChanged();
            finishConnecting(std::move(device));
        } else {
            emit errorMessageChanged();
        }
        return;
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
    roiSupported_ = false;
    sensorWidth_ = 0;
    sensorHeight_ = 0;
    currentConfiguration_ = astro::domain::StreamConfiguration{};
    extraControls_.clear();

    connected_ = false;
    cameraName_.clear();
    frameCount_.store(0, std::memory_order_relaxed);
    renderedFrameCount_.store(0, std::memory_order_relaxed);
    lastRenderedSequence_.reset();
    cachedFrameImage_ = QImage();
    previewTimer_.stop();

    emit connectedChanged();
    emit frameCountChanged();
    emit exposureChanged();
    emit gainChanged();
    emit roiChanged();
    emit extraControlsChanged();
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

QVariantList PreviewController::extraControls() const {
    QVariantList list;
    for (const auto& control : extraControls_) {
        QVariantMap entry;
        entry[QStringLiteral("key")] = control.key;
        entry[QStringLiteral("label")] = control.label;
        entry[QStringLiteral("unit")] = control.unit;
        entry[QStringLiteral("isFloat")] = control.isFloat;
        entry[QStringLiteral("value")] = control.value;
        entry[QStringLiteral("min")] = control.minValue;
        entry[QStringLiteral("max")] = control.maxValue;
        entry[QStringLiteral("step")] = control.step;
        list.append(entry);
    }
    return list;
}

void PreviewController::setExtraControlValue(const QString& key, double value) {
    if (!device_) {
        return;
    }
    for (auto& control : extraControls_) {
        if (control.key != key) {
            continue;
        }
        const double clamped = std::clamp(value, control.minValue, control.maxValue);
        const ControlValue controlValue =
            control.isFloat ? ControlValue{clamped} : ControlValue{static_cast<std::int64_t>(std::llround(clamped))};
        if (const auto result = device_->setControl(control.id, controlValue); !result.ok()) {
            errorMessage_ = QString::fromStdString(result.error().message);
            emit errorMessageChanged();
            return;
        }
        control.value = clamped;
        settings_.saveExtraControl(QString::fromStdString(device_->descriptor().id), key, clamped);
        // Deliberately no extraControlsChanged() here: that property backs
        // a QML Repeater over a QVariantList, and re-emitting it on every
        // slider-drag update replaces the whole list — which destroys and
        // recreates every delegate (including the one being dragged),
        // aborting the drag gesture entirely (only a click-to-position
        // would "stick"). The slider driving this call already reflects
        // its own live value locally; nothing else needs to be notified
        // (extraControls values aren't part of presets, and no other UI
        // reads them).
        return;
    }
}

void PreviewController::setRoi(int x, int y, int width, int height) {
    if (!connected_ || !device_ || !roiSupported_ || recording_) {
        return;
    }

    StreamConfiguration newConfiguration = currentConfiguration_;
    newConfiguration.roi = Roi{static_cast<std::uint32_t>(std::max(0, x)), static_cast<std::uint32_t>(std::max(0, y)),
                                static_cast<std::uint32_t>(std::max(0, width)),
                                static_cast<std::uint32_t>(std::max(0, height))};

    // Real backends generally require the stream stopped to reconfigure;
    // the mock enforces this too (configure() rejects while running), so
    // this stop/configure/start sequence is what a real ROI-capable
    // backend will need as well, not a mock-only shortcut.
    device_->stop();
    if (const auto result = device_->configure(newConfiguration); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
        // Best-effort: restart with the last-known-good configuration so a
        // rejected ROI doesn't leave the camera stopped.
        device_->configure(currentConfiguration_);
        device_->start(*this);
        emit errorMessageChanged();
        return;
    }
    if (const auto result = device_->start(*this); !result.ok()) {
        errorMessage_ = QString::fromStdString(result.error().message);
        emit errorMessageChanged();
        return;
    }

    currentConfiguration_ = newConfiguration;
    frameSlot_.clear();
    lastRenderedSequence_.reset();
    cachedFrameImage_ = QImage();
    emit roiChanged();
}

void PreviewController::resetRoi() { setRoi(0, 0, 0, 0); }

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
        // Debayering is the expensive part of this call; LivePreviewItem
        // and ZoomLoupeItem both call frame() independently on the same
        // previewTick, so without this cache an unchanged frame was being
        // fully re-debayered twice per tick for no reason.
        cachedFrameImage_ = frameToPreviewImage(latest->descriptor, latest->storage->data(), latest->storage->size());
    }
    return cachedFrameImage_;
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
    const QString extension = saveFormat_ == QStringLiteral("TIFF16")   ? QStringLiteral("tiff")
                               : saveFormat_ == QStringLiteral("FITS")  ? QStringLiteral("fits")
                                                                        : QStringLiteral("png");
    const QString path = directory + QLatin1Char('/') + timestampedBaseName(cameraId, targetName_) +
                          QLatin1Char('.') + extension;

    bool saved = false;
    if (saveFormat_ == QStringLiteral("PNG")) {
        saved = image.save(path, "PNG");
    } else {
        // Both writers need tightly packed (no row padding) interleaved
        // RGB8 bytes; QImage's own bytesPerLine() can pad each scanline,
        // so copy row by row rather than assuming constBits() is already
        // contiguous.
        const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
        std::vector<std::uint8_t> tightRgb(static_cast<std::size_t>(rgb.width()) * rgb.height() * 3);
        for (int y = 0; y < rgb.height(); ++y) {
            std::memcpy(tightRgb.data() + static_cast<std::size_t>(y) * rgb.width() * 3, rgb.constScanLine(y),
                        static_cast<std::size_t>(rgb.width()) * 3);
        }

        const auto result = saveFormat_ == QStringLiteral("TIFF16")
                                 ? astro::recording::writeTiff16Rgb(path.toStdString(),
                                                                     static_cast<std::uint32_t>(rgb.width()),
                                                                     static_cast<std::uint32_t>(rgb.height()),
                                                                     tightRgb.data())
                                 : astro::recording::writeFits8Rgb(path.toStdString(),
                                                                    static_cast<std::uint32_t>(rgb.width()),
                                                                    static_cast<std::uint32_t>(rgb.height()),
                                                                    tightRgb.data());
        saved = result.ok();
    }

    if (!saved) {
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
    const bool useSer = videoFormat_ == QStringLiteral("SER");
    const QString path = directory + QLatin1Char('/') + timestampedBaseName(cameraId, targetName_) +
                          (useSer ? QStringLiteral(".ser") : QStringLiteral(".acraw"));

    std::unique_ptr<astro::recording::IRecorder> recorder =
        useSer ? std::unique_ptr<astro::recording::IRecorder>(std::make_unique<astro::recording::SerRecorder>())
               : std::unique_ptr<astro::recording::IRecorder>(
                     std::make_unique<astro::recording::RawSequenceRecorder>());
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

void PreviewController::openSaveDirectory() {
    const QString path = saveDirectory_.isEmpty() ? imagesDirectoryPath() : saveDirectory_;
    QDir().mkpath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void PreviewController::setSaveFormat(const QString& format) {
    if (saveFormat_ == format ||
        (format != QStringLiteral("PNG") && format != QStringLiteral("TIFF16") && format != QStringLiteral("FITS"))) {
        return;
    }
    saveFormat_ = format;
    settings_.saveImageFormat(format);
    emit saveFormatChanged();
}

void PreviewController::setVideoFormat(const QString& format) {
    if (recording_ || videoFormat_ == format ||
        (format != QStringLiteral("SER") && format != QStringLiteral("ACRAW"))) {
        return;
    }
    videoFormat_ = format;
    settings_.saveVideoFormat(format);
    emit videoFormatChanged();
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
