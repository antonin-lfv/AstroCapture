#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <QImage>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

#include "Settings.hpp"
#include "astro/domain/BoundedFrameQueue.hpp"
#include "astro/domain/CameraControl.hpp"
#include "astro/domain/Frame.hpp"
#include "astro/domain/ICameraBackend.hpp"
#include "astro/domain/ICameraDevice.hpp"
#include "astro/domain/LatestFrameSlot.hpp"
#include "astro/processing/Histogram.hpp"
#include "astro/recording/IRecorder.hpp"

namespace astro::app {

// Owns a single opened camera and republishes its frames as QImages for QML.
// Stands in for the future CaptureSession/CameraManager pair (see
// docs/development/IMPLEMENTATION_BLUEPRINT.md sections 7-8); deliberately
// supports one device at a time. rescan() tries compiled-in backends in
// priority order (real hardware before the mock fallback).
class PreviewController : public QObject, private astro::domain::FrameSink {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString cameraName READ cameraName NOTIFY connectedChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(quint64 frameCount READ frameCount NOTIFY frameCountChanged)
    // Diagnostics (ROADMAP.md Gate B: "acquisition statistics visible in
    // dev diagnostics"). previewSkippedFrames counts frames LatestFrameSlot
    // overwrote before the preview ever rendered them — expected/by-design
    // for a live preview, not an error, but worth surfacing.
    Q_PROPERTY(double fps READ fps NOTIFY frameCountChanged)
    Q_PROPERTY(quint64 previewSkippedFrames READ previewSkippedFrames NOTIFY frameCountChanged)

    // Camera controls (AGENT.md section 8: capability-driven, never assume
    // a control exists). Only Exposure/Gain exist as canonical controls
    // today, so these are direct typed properties rather than a generic
    // list model; generalize into a real CameraControlModel (blueprint
    // section 11) once a third control needs exposing.
    Q_PROPERTY(bool exposureSupported READ exposureSupported NOTIFY connectedChanged)
    Q_PROPERTY(double exposureMicroseconds READ exposureMicroseconds WRITE setExposureMicroseconds NOTIFY
                   exposureChanged)
    Q_PROPERTY(double exposureMinMicroseconds READ exposureMinMicroseconds NOTIFY connectedChanged)
    Q_PROPERTY(double exposureMaxMicroseconds READ exposureMaxMicroseconds NOTIFY connectedChanged)

    Q_PROPERTY(bool gainSupported READ gainSupported NOTIFY connectedChanged)
    Q_PROPERTY(int gainValue READ gainValue WRITE setGainValue NOTIFY gainChanged)
    Q_PROPERTY(int gainMin READ gainMin NOTIFY connectedChanged)
    Q_PROPERTY(int gainMax READ gainMax NOTIFY connectedChanged)

    Q_PROPERTY(QString lastCaptureStatus READ lastCaptureStatus NOTIFY lastCaptureStatusChanged)

    // Recording (ROADMAP.md Gate D: video recording + "no silent dropped
    // frames"). recordingDroppedFrames counts BoundedFrameQueue overflows —
    // must stay visible, never silently absorbed.
    Q_PROPERTY(bool recording READ recording NOTIFY recordingChanged)
    Q_PROPERTY(QString recordingPath READ recordingPath NOTIFY recordingChanged)
    Q_PROPERTY(quint64 recordingFrameCount READ recordingFrameCount NOTIFY recordingStatsChanged)
    Q_PROPERTY(quint64 recordingDroppedFrames READ recordingDroppedFrames NOTIFY recordingStatsChanged)
    Q_PROPERTY(double recordingElapsedSeconds READ recordingElapsedSeconds NOTIFY recordingStatsChanged)
    Q_PROPERTY(QString recordingStatus READ recordingStatus NOTIFY recordingStatusChanged)

    // Empty means "use the OS default" (Pictures/AstroCapture,
    // Movies/AstroCapture); once set, both captures and recordings go under
    // <saveDirectory>/Images and <saveDirectory>/Videos instead.
    Q_PROPERTY(QString saveDirectory READ saveDirectory WRITE setSaveDirectory NOTIFY saveDirectoryChanged)
    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY presetsChanged)

public:
    explicit PreviewController(QObject* parent = nullptr);
    ~PreviewController() override;

    bool connected() const { return connected_; }
    QString cameraName() const { return cameraName_; }
    QString errorMessage() const { return errorMessage_; }
    quint64 frameCount() const { return frameCount_.load(std::memory_order_relaxed); }
    double fps() const;
    quint64 previewSkippedFrames() const;

    bool exposureSupported() const { return exposureCapability_.has_value(); }
    double exposureMicroseconds() const { return exposureMicroseconds_; }
    double exposureMinMicroseconds() const { return exposureCapability_ ? exposureCapability_->minValue : 0.0; }
    double exposureMaxMicroseconds() const { return exposureCapability_ ? exposureCapability_->maxValue : 0.0; }
    void setExposureMicroseconds(double value);

    bool gainSupported() const { return gainCapability_.has_value(); }
    int gainValue() const { return static_cast<int>(gainValue_); }
    int gainMin() const { return gainCapability_ ? static_cast<int>(gainCapability_->minValue) : 0; }
    int gainMax() const { return gainCapability_ ? static_cast<int>(gainCapability_->maxValue) : 0; }
    void setGainValue(int value);

    // Debayers the latest frame on the calling thread. Intended for
    // LivePreviewItem to call from the render/GUI thread, never from the
    // camera's acquisition thread.
    QImage frame() const;

    // Computes a histogram from the latest raw frame on the calling thread.
    // Same threading contract as frame(): render/GUI thread only, intended
    // for HistogramItem.
    astro::processing::Histogram histogram() const;

    QString lastCaptureStatus() const { return lastCaptureStatus_; }

    bool recording() const { return recording_; }
    QString recordingPath() const { return recordingPath_; }
    quint64 recordingFrameCount() const { return recordingFrameCount_.load(std::memory_order_relaxed); }
    quint64 recordingDroppedFrames() const { return recordingDroppedFrames_.load(std::memory_order_relaxed); }
    double recordingElapsedSeconds() const;
    QString recordingStatus() const { return recordingStatus_; }

    Q_INVOKABLE void rescan();

    // Stops the current device (and any active recording) and returns to
    // the disconnected/empty state, so rescan() can pick a different
    // camera. Without this, rescan() no-ops once already connected.
    Q_INVOKABLE void disconnectCamera();

    // Saves the currently displayed frame (i.e. frame()'s output, already
    // debayered/converted) as PNG under Pictures/AstroCapture. Sets
    // lastCaptureStatus to the saved path or a failure reason.
    Q_INVOKABLE void captureStillImage();

    // Starts writing raw frames to Movies/AstroCapture/{date}_{time}_
    // {camera}.acraw (see RawSequenceRecorder) until stopRecording() is
    // called. Recording runs on its own writer thread, fed through a
    // bounded queue, so it never blocks acquisition or preview.
    Q_INVOKABLE void startRecording();
    Q_INVOKABLE void stopRecording();

    QString saveDirectory() const { return saveDirectory_; }
    void setSaveDirectory(const QString& path);

    // Named presets store the current camera's exposure/gain; scoped per
    // camera id (like the last-used values) since valid ranges are
    // hardware-specific.
    QStringList presetNames() const;
    Q_INVOKABLE void savePreset(const QString& name);
    Q_INVOKABLE void loadPreset(const QString& name);
    Q_INVOKABLE void deletePreset(const QString& name);

signals:
    void connectedChanged();
    void errorMessageChanged();
    void frameCountChanged();
    void frameChanged();
    void exposureChanged();
    void gainChanged();
    void lastCaptureStatusChanged();
    void recordingChanged();
    void recordingStatsChanged();
    void recordingStatusChanged();
    void saveDirectoryChanged();
    void presetsChanged();

private:
    // astro::domain::FrameSink — called on the camera's acquisition thread.
    // Must stay cheap: no decoding, no Qt GUI work (AGENT.md 2.2, the
    // acquisition path must never be blocked by preview/UI work).
    void onFrame(const astro::domain::FrameBuffer& frameBuffer) override;

    // Opens, configures and starts the first device `backend` enumerates.
    // Returns nullptr (with errorMessage_ set only on a real failure, left
    // untouched if the backend simply found no devices) if unsuccessful.
    std::unique_ptr<astro::domain::ICameraDevice> tryOpenFirstDevice(astro::domain::ICameraBackend& backend);

    // Resolve to saveDirectory_/Images or .../Videos when set, otherwise
    // the OS-standard Pictures/AstroCapture or Movies/AstroCapture.
    QString imagesDirectoryPath() const;
    QString videosDirectoryPath() const;

    std::unique_ptr<astro::domain::ICameraDevice> device_;
    astro::domain::LatestFrameSlot frameSlot_;

    // Guards recordingQueue_ against the race between onFrame() (checks/
    // pushes from the acquisition thread) and start/stopRecording() (create/
    // destroy from the GUI thread). The writer thread itself never touches
    // this member directly — it operates on a raw pointer captured at
    // thread-start, see startRecording()'s comment for why that matters.
    std::mutex recordingMutex_;
    std::unique_ptr<astro::domain::BoundedFrameQueue> recordingQueue_;
    std::unique_ptr<astro::recording::IRecorder> recorder_;
    std::thread recordingThread_;
    bool recording_ = false;
    QString recordingPath_;
    QString recordingStatus_;
    std::atomic<std::uint64_t> recordingFrameCount_{0};
    std::atomic<std::uint64_t> recordingDroppedFrames_{0};
    std::chrono::steady_clock::time_point recordingStartTime_;  // GUI thread only

    Settings settings_;
    QString saveDirectory_;
    std::optional<astro::domain::CameraCapability> exposureCapability_;
    double exposureMicroseconds_ = 0.0;
    std::optional<astro::domain::CameraCapability> gainCapability_;
    std::int64_t gainValue_ = 0;

    bool connected_ = false;
    QString cameraName_;
    QString errorMessage_;
    QString lastCaptureStatus_;
    std::atomic<std::uint64_t> frameCount_{0};
    std::chrono::steady_clock::time_point startTime_;  // GUI thread only (set in rescan())

    // Touched only from frame(), which callers must only invoke from the
    // render/GUI thread, so these are not atomic despite being mutable.
    mutable std::optional<std::uint64_t> lastRenderedSequence_;
    mutable std::atomic<std::uint64_t> renderedFrameCount_{0};
};

}  // namespace astro::app
