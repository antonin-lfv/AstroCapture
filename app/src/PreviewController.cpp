#include "PreviewController.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include <QDesktopServices>
#include <QSaveFile>
#include <QTemporaryDir>
#include <astro/processing/DeepSky.hpp>
#include <astro/recording/FitsSequenceRecorder.hpp>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>

#include "FileNaming.hpp"
#include "FramePreviewImage.hpp"
#include "astro/domain/StreamMode.hpp"
#include "astro/processing/Debayer.hpp"
#include "astro/recording/FitsWriter.hpp"
#include "astro/recording/RawSequenceRecorder.hpp"
#include "astro/recording/SerRecorder.hpp"
#include "astro/recording/TiffWriter.hpp"

#ifdef ASTRO_ENABLE_MOCK_CAMERA
#include "astro/driver/mock/MockCameraBackend.hpp"
#endif

#ifdef ASTRO_ENABLE_DRIVER_UVC
#include "CameraPermission.hpp"
#include "astro/driver/uvc/UvcCameraBackend.hpp"
#endif

#ifdef ASTRO_ENABLE_DRIVER_EUVC
#include "astro/driver/euvc/EuvcCameraBackend.hpp"
#endif

#ifdef ASTRO_ENABLE_DRIVER_ZWO
#include "astro/driver/zwo/ZwoCameraBackend.hpp"
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
            label = QString::fromUtf8(QT_TRANSLATE_NOOP("CameraControls", "Gamma"));
            unit.clear();
            isFloat = true;
            return true;
        case ControlId::WhiteBalanceRed:
            key = QStringLiteral("whiteBalanceRed");
            label = QString::fromUtf8(QT_TRANSLATE_NOOP("CameraControls", "White Balance (R)"));
            unit.clear();
            isFloat = false;
            return true;
        case ControlId::WhiteBalanceBlue:
            key = QStringLiteral("whiteBalanceBlue");
            label = QString::fromUtf8(QT_TRANSLATE_NOOP("CameraControls", "White Balance (B)"));
            unit.clear();
            isFloat = false;
            return true;
        case ControlId::Brightness:
            key = QStringLiteral("brightness");
            label = QString::fromUtf8(QT_TRANSLATE_NOOP("CameraControls", "Brightness"));
            unit.clear();
            isFloat = false;
            return true;
        case ControlId::FrameRateLimit:
            key = QStringLiteral("frameRateLimit");
            label = QString::fromUtf8(QT_TRANSLATE_NOOP("CameraControls", "Frame Rate Limit"));
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
    videoFormat_ = (loadedVideoFormat == QStringLiteral("ACRAW") || loadedVideoFormat == QStringLiteral("FITS")) ? loadedVideoFormat : QStringLiteral("SER");
    // The writer thread can't call stopRecording() itself (it would try to
    // join its own thread), so it emits recordingTargetReached() instead;
    // Qt queues delivery to this object's thread (GUI) automatically.
    connect(this, &PreviewController::recordingTargetReached, this, [this](quint64 generation) {
        if (generation == recordingGeneration_) stopRecording();
    }, Qt::QueuedConnection);

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
    connect(&previewTimer_, &QTimer::timeout, this, [this] {
        emit frameCountChanged();
        if (recording_) emit recordingStatsChanged();
    });
    recordingDeadlineTimer_.setSingleShot(true);
    connect(&recordingDeadlineTimer_, &QTimer::timeout, this, &PreviewController::stopRecording);
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
        errorMessage_ = tr("Failed to open camera.");
        return nullptr;
    }

    const auto modes = device->streamModes();
    if (modes.empty()) {
        errorMessage_ = tr("Camera exposes no stream modes.");
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
#ifdef ASTRO_ENABLE_DRIVER_ZWO
    if (candidate.backendId == QStringLiteral("zwo")) {
        astro::driver::zwo::ZwoCameraBackend backend;
        return openAndStart(backend, candidate.id.toStdString());
    }
#endif
#ifdef ASTRO_ENABLE_DRIVER_EUVC
    if (candidate.backendId == QStringLiteral("euvc")) {
        astro::driver::euvc::EuvcCameraBackend backend;
        return openAndStart(backend, candidate.id.toStdString());
    }
#endif
#ifdef ASTRO_ENABLE_DRIVER_UVC
    if (candidate.backendId == QStringLiteral("uvc")) {
        astro::driver::uvc::UvcCameraBackend backend;
        return openAndStart(backend, candidate.id.toStdString());
    }
#endif
#ifdef ASTRO_ENABLE_MOCK_CAMERA
    if (candidate.backendId == QStringLiteral("mock")) {
        astro::driver::mock::MockCameraBackend backend;
        return openAndStart(backend, candidate.id.toStdString());
    }
#endif
    errorMessage_ = tr("Camera backend not available in this build.");
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
    // picker (real + mock) — see the class comment.
    std::vector<CameraCandidate> realCandidates;

#ifdef ASTRO_ENABLE_DRIVER_UVC
    // macOS gates raw USB Video-class access behind the same TCC privacy
    // permission as AVFoundation (ADR-005) — skip enumerate() rather than
    // let it silently find nothing when permission was never granted.
    // NotDetermined fires the system prompt but doesn't wait for it (see
    // CameraPermission.hpp): this rescan() still falls through to mock,
    // and a later rescan() (after the user responds) picks up the answer.
    if (astro::app::cameraAuthorizationStatus() == astro::app::CameraAuthStatus::NotDetermined) {
        astro::app::requestCameraAuthorization();
    }
    if (astro::app::cameraAuthorizationStatus() == astro::app::CameraAuthStatus::Authorized) {
        astro::driver::uvc::UvcCameraBackend backend;
        for (const auto& descriptor : backend.enumerate()) {
            realCandidates.push_back({QString::fromStdString(descriptor.id),
                                       QString::fromStdString(descriptor.displayName), QStringLiteral("uvc")});
        }
    }
#endif

#ifdef ASTRO_ENABLE_DRIVER_EUVC
    // No macOS permission gate here (ADR-006): this camera family's
    // control/streaming interfaces declare a vendor-specific class, not
    // UVC, so nothing in the OS claims or gates access to them the way
    // UVCAssistant does for drivers/uvc above.
    {
        astro::driver::euvc::EuvcCameraBackend backend;
        for (const auto& descriptor : backend.enumerate()) {
            realCandidates.push_back({QString::fromStdString(descriptor.id),
                                       QString::fromStdString(descriptor.displayName), QStringLiteral("euvc")});
        }
    }
#endif

#ifdef ASTRO_ENABLE_DRIVER_ZWO
    // No macOS permission gate here either (same reasoning as euvc above):
    // ZWO's ASICamera2 SDK talks to the device directly, not through any
    // OS-recognized camera class.
    {
        astro::driver::zwo::ZwoCameraBackend backend;
        for (const auto& descriptor : backend.enumerate()) {
            realCandidates.push_back({QString::fromStdString(descriptor.id),
                                       QString::fromStdString(descriptor.displayName), QStringLiteral("zwo")});
        }
    }
#endif

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
        if (!backend.enumerate().empty()) {
            emit mockCameraOffered();
            return;
        }
    }
#endif

    if (errorMessage_.isEmpty()) {
        errorMessage_ = tr("No camera found.");
    }
    emit errorMessageChanged();
}

void PreviewController::connectMock() {
#ifdef ASTRO_ENABLE_MOCK_CAMERA
    if (connected_) {
        return;
    }
    astro::driver::mock::MockCameraBackend backend;
    const auto descriptors = backend.enumerate();
    if (descriptors.empty()) {
        return;
    }
    const CameraCandidate mockCandidate{QString::fromStdString(descriptors.front().id),
                                         QString::fromStdString(descriptors.front().displayName),
                                         QStringLiteral("mock")};
    if (auto device = openCandidate(mockCandidate)) {
        finishConnecting(std::move(device));
    } else {
        emit errorMessageChanged();
    }
#endif
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
    lastPreviewSequence_.reset();
    cachedPreviewImage_ = QImage();
    previewTimer_.stop();

    // A leftover error (e.g. a failed control write) must not linger on
    // the empty/disconnected state once the user has moved past it —
    // rescan() already clears this at its own start for the same reason.
    errorMessage_.clear();

    emit connectedChanged();
    emit errorMessageChanged();
    emit frameCountChanged();
    emit exposureChanged();
    emit gainChanged();
    emit roiChanged();
    emit extraControlsChanged();
}

void PreviewController::setExposureMicroseconds(double value) {
    if (recording_ || !device_ || !exposureCapability_) {
        return;
    }
    // Clamp rather than trust the caller: the QML slider computes this via
    // a log10/pow10 round-trip for its logarithmic scale, which can
    // overshoot min/max by a floating-point epsilon and fail the
    // backend's strict range check otherwise.
    value = std::clamp(value, exposureCapability_->minValue, exposureCapability_->maxValue);
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
    if (recording_ || !device_ || !gainCapability_) {
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
    if (recording_ || !device_) {
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
    lastPreviewSequence_.reset();
    cachedPreviewImage_ = QImage();
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
        // Debayering is the expensive part of this call; ZoomLoupeItem can
        // call this more than once per tick while active, so without this
        // cache an unchanged frame was being fully re-debayered for no
        // reason.
        cachedFrameImage_ = frameToPreviewImage(latest->descriptor, latest->storage->data(), latest->storage->size());
    }
    return cachedFrameImage_;
}

QImage PreviewController::previewImage() const {
    const auto latest = frameSlot_.latest();
    if (!latest) {
        return QImage();
    }
    if (lastPreviewSequence_ != latest->metadata.sequence) {
        lastPreviewSequence_ = latest->metadata.sequence;
        renderedFrameCount_.fetch_add(1, std::memory_order_relaxed);

        // Only real sensor frames need decimating — the mock's 640x480
        // test pattern is already cheap to debayer in full. Picks the
        // smallest even factor that gets the long edge under ~1000px,
        // which is comfortably more detail than a sidebar-width preview
        // pane can show anyway.
        constexpr std::uint32_t kPreviewTargetMaxDimension = 1000;
        std::uint32_t factor = 1;
        while (std::max(latest->descriptor.width, latest->descriptor.height) / factor > kPreviewTargetMaxDimension &&
               factor < 8) {
            factor *= 2;
        }

        if (latest->descriptor.storage == PixelStorage::Bayer8 && factor > 1) {
            const auto decimated = astro::processing::decimateBayer(latest->descriptor, latest->storage->data(),
                                                                      latest->storage->size(), factor);
            cachedPreviewImage_ = frameToPreviewImage(decimated.descriptor, decimated.data.data(), decimated.data.size());
        } else {
            cachedPreviewImage_ =
                frameToPreviewImage(latest->descriptor, latest->storage->data(), latest->storage->size());
        }
    }
    return cachedPreviewImage_;
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
        lastCaptureStatus_ = tr("No camera connected.");
        emit lastCaptureStatusChanged();
        return;
    }

    const QImage image = frame();
    if (image.isNull()) {
        lastCaptureStatus_ = tr("No frame available yet.");
        emit lastCaptureStatusChanged();
        return;
    }

    const QString directory = imagesDirectoryPath();
    if (!QDir().mkpath(directory)) {
        lastCaptureStatus_ = tr("Failed to create ") + directory;
        emit lastCaptureStatusChanged();
        return;
    }

    const QString cameraId = QString::fromStdString(device_->descriptor().id);
    const QString extension = saveFormat_ == QStringLiteral("TIFF16")   ? QStringLiteral("tiff")
                               : saveFormat_ == QStringLiteral("FITS")  ? QStringLiteral("fits")
                                                                        : QStringLiteral("png");
    const QString path = reserveCapturePath(directory, timestampedBaseName(cameraId, targetName_), extension);
    if (path.isEmpty()) {
        lastCaptureStatus_ = tr("Failed to create image file.");
        emit lastCaptureStatusChanged();
        return;
    }

    bool saved = false;
    if (saveFormat_ == QStringLiteral("PNG")) {
        saved = image.save(path, "PNG");
    } else {
        try {
            const auto raw = frameSlot_.latest();
            if (!raw) throw std::runtime_error("No camera frame available.");
            auto data = astro::recording::imageFromFrame(*raw);
            data.exposure = exposureMicroseconds_ / 1000000.0;
            data.gain = double(gainValue_); data.hasGain = gainSupported();
            data.object = targetName_.toStdString();
            if (saveFormat_ == QStringLiteral("FITS")) {
                QTemporaryDir temporary(QFileInfo(path).dir().filePath(".capture-XXXXXX"));
                if (!temporary.isValid()) throw std::runtime_error("Cannot create capture temporary directory.");
                const auto tempPath = temporary.filePath("image.fits");
                astro::recording::writeAstroFits(tempPath.toStdString(), data);
                QFile input(tempPath); QSaveFile output(path);
                if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly)) throw std::runtime_error("Cannot save FITS capture.");
                while (!input.atEnd()) {const auto block=input.read(1024*1024);if(block.isEmpty()||output.write(block)!=block.size())throw std::runtime_error("Cannot write FITS capture.");}
                saved = output.commit();
            } else {
                const auto rgb = astro::processing::linearRgb(data);
                std::vector<std::uint16_t> samples(rgb.rgb.size());
                for(std::size_t i=0;i<samples.size();++i) samples[i]=std::uint16_t(std::lround(std::clamp(rgb.rgb[i],0.f,1.f)*65535));
                saved = astro::recording::writeTiff16Samples(path.toStdString(),rgb.width,rgb.height,samples.data()).ok();
            }
        } catch(const std::exception& error) {
            lastCaptureStatus_ = QString::fromUtf8(error.what());
            emit lastCaptureStatusChanged();
            return;
        }
    }

    if (!saved) {
        lastCaptureStatus_ = tr("Failed to save image.");
        emit lastCaptureStatusChanged();
        return;
    }

    lastCaptureStatus_ = tr("Saved ") + path;
    emit lastCaptureStatusChanged();
    emit stillImageSaved(QUrl::fromLocalFile(path));
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
        recordingStatus_ = tr("No camera connected.");
        emit recordingStatusChanged();
        return;
    }
    if (recording_) {
        return;
    }

    const auto latest = frameSlot_.latest();
    if (!latest) {
        recordingStatus_ = tr("No frame available yet.");
        emit recordingStatusChanged();
        return;
    }

    const QString directory = videosDirectoryPath();
    if (!QDir().mkpath(directory)) {
        recordingStatus_ = tr("Failed to create ") + directory;
        emit recordingStatusChanged();
        return;
    }

    const QString cameraId = QString::fromStdString(device_->descriptor().id);
    const bool useSer = videoFormat_ == QStringLiteral("SER");
    const bool useFits = videoFormat_ == QStringLiteral("FITS");
    const QString path = reserveCapturePath(directory, timestampedBaseName(cameraId, targetName_),
                                            useSer ? QStringLiteral("ser") : useFits ? QStringLiteral("fits-sequence") : QStringLiteral("acraw"));
    if (path.isEmpty()) {
        recordingStatus_ = tr("Failed to create recording file.");
        emit recordingStatusChanged();
        return;
    }

    std::unique_ptr<astro::recording::IRecorder> recorder;
    if (useFits) {
        QFile::remove(path); // replace the reserved placeholder with a new directory
        auto fits = std::make_unique<astro::recording::FitsSequenceRecorder>();
        fits->metadata.exposure = exposureMicroseconds_ / 1000000.0;
        fits->metadata.gain = double(gainValue_); fits->metadata.hasGain = gainSupported();
        fits->metadata.object = targetName_.toStdString();
        fits->metadata.filter = captureFilter_.toStdString();
        fits->metadata.imageType = captureType_.toStdString();
        recorder = std::move(fits);
    } else if(useSer) recorder = std::make_unique<astro::recording::SerRecorder>();
    else recorder = std::make_unique<astro::recording::RawSequenceRecorder>();
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
    recordingFrameCount_.store(0, std::memory_order_relaxed);
    recordingDroppedFrames_.store(0, std::memory_order_relaxed);
    recordingFailure_.clear();
    {
        std::lock_guard lock(recordingMutex_);
        recordingQueue_ = std::move(queue);
        recordingAcceptingFrames_ = true;
    }
    recorder_ = std::move(recorder);

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
    const auto generation = ++recordingGeneration_;
    if (targetDurationSeconds > 0.0) {
        // A stalled/disconnected camera cannot provide the next frame to
        // check the deadline. Keep a GUI timer as an independent stop.
        recordingDeadlineTimer_.start(std::chrono::milliseconds(
            static_cast<std::int64_t>(std::min(targetDurationSeconds * 1000.0, 2147483647.0))));
    }

    recordingThread_ = std::thread([this, queuePtr, recorderPtr, targetFrameCount, targetDurationSeconds,
                                     recordingStart, generation, useFits, exposureSeconds = exposureMicroseconds_ / 1000000.0] {
        while (auto queuedFrame = queuePtr->waitPop()) {
            if (useFits && std::chrono::duration<double>(queuedFrame->metadata.hostArrival - recordingStart).count() < exposureSeconds) continue;
            const auto result = recorderPtr->writeFrame(*queuedFrame);
            std::uint64_t written = 0;
            if (result.ok()) {
                written = recordingFrameCount_.fetch_add(1, std::memory_order_relaxed) + 1;
            } else {
                recordingDroppedFrames_.fetch_add(1, std::memory_order_relaxed);
                recordingFailure_ = result.error().message;
            }

            const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - recordingStart)
                                        .count();
            const bool frameTargetReached = targetFrameCount > 0 && written >= static_cast<std::uint64_t>(targetFrameCount);
            const bool durationTargetReached = targetDurationSeconds > 0.0 && elapsed >= targetDurationSeconds;
            if (!result.ok() || frameTargetReached || durationTargetReached) {
                // Seal admission immediately. Waiting for the GUI's stop
                // would report spurious drops after a completed target.
                {
                    std::lock_guard lock(recordingMutex_);
                    recordingAcceptingFrames_ = false;
                }
                emit recordingTargetReached(generation);
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
    recordingDeadlineTimer_.stop();

    // Order matters: stop() wakes the writer thread's waitPop() so it can
    // drain whatever's left and exit; join() guarantees it has actually
    // exited (and so is done touching queuePtr/recorderPtr) before we
    // destroy the objects those pointers reference below.
    // Detach under the producer's lock before draining: new camera frames
    // belong to the preview, not to the recording that is being finalized.
    std::unique_ptr<astro::domain::BoundedFrameQueue> finishingQueue;
    {
        std::lock_guard lock(recordingMutex_);
        recordingAcceptingFrames_ = false;
        finishingQueue = std::move(recordingQueue_);
    }
    finishingQueue->stop();
    if (recordingThread_.joinable()) {
        recordingThread_.join();
    }

    const auto summary = recorder_->close();
    if (!recordingFailure_.empty()) {
        recordingStatus_ = tr("Recording failed: ") + QString::fromStdString(recordingFailure_);
    } else if (summary.ok()) {
        recordingStatus_ = tr("Saved ") + recordingPath_ + QStringLiteral(" (") +
                            QString::number(summary.value().framesWritten) + tr(" frames, ") +
                            QString::number(recordingDroppedFrames_.load(std::memory_order_relaxed)) +
                            tr(" dropped)");
    } else {
        recordingStatus_ = QString::fromStdString(summary.error().message);
    }
    recorder_.reset();

    recording_ = false;
    emit recordingChanged();
    emit recordingStatusChanged();
    emit recordingStatsChanged();
}

void PreviewController::onFrame(const FrameBuffer& frameBuffer) {
    frameSlot_.store(frameBuffer);
    frameCount_.fetch_add(1, std::memory_order_relaxed);

    {
        std::lock_guard lock(recordingMutex_);
        if (recordingAcceptingFrames_ && recordingQueue_ && !recordingQueue_->tryPush(frameBuffer)) {
            recordingDroppedFrames_.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Signal emission is thread-safe in Qt: connections to GUI-thread
    // receivers (LivePreviewItem, QML property bindings) are queued
    // automatically, so this stays cheap on the acquisition thread.
    emit frameChanged();
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

QUrl PreviewController::saveDirectoryUrl() const {
    return QUrl::fromLocalFile(saveDirectory_.isEmpty() ? imagesDirectoryPath() : saveDirectory_);
}

void PreviewController::chooseSaveDirectory(const QUrl& url) {
    if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isDir())
        setSaveDirectory(QFileInfo(url.toLocalFile()).absoluteFilePath());
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
        (format != QStringLiteral("SER") && format != QStringLiteral("ACRAW") && format != QStringLiteral("FITS"))) {
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
