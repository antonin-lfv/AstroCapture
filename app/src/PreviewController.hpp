#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

#include <QImage>
#include <QObject>
#include <QQmlEngine>
#include <QString>

#include "astro/domain/Frame.hpp"
#include "astro/domain/ICameraDevice.hpp"
#include "astro/domain/LatestFrameSlot.hpp"

namespace astro::app {

// Owns a single opened camera and republishes its frames as QImages for QML.
// Stands in for the future CaptureSession/CameraManager pair (see
// docs/development/IMPLEMENTATION_BLUEPRINT.md sections 7-8) while only one
// backend (mock) exists; deliberately supports one device at a time.
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

public:
    explicit PreviewController(QObject* parent = nullptr);
    ~PreviewController() override;

    bool connected() const { return connected_; }
    QString cameraName() const { return cameraName_; }
    QString errorMessage() const { return errorMessage_; }
    quint64 frameCount() const { return frameCount_.load(std::memory_order_relaxed); }
    double fps() const;
    quint64 previewSkippedFrames() const;

    // Debayers the latest frame on the calling thread. Intended for
    // LivePreviewItem to call from the render/GUI thread, never from the
    // camera's acquisition thread.
    QImage frame() const;

    Q_INVOKABLE void rescan();

signals:
    void connectedChanged();
    void errorMessageChanged();
    void frameCountChanged();
    void frameChanged();

private:
    // astro::domain::FrameSink — called on the camera's acquisition thread.
    // Must stay cheap: no decoding, no Qt GUI work (AGENT.md 2.2, the
    // acquisition path must never be blocked by preview/UI work).
    void onFrame(const astro::domain::FrameBuffer& frameBuffer) override;

    std::unique_ptr<astro::domain::ICameraDevice> device_;
    astro::domain::LatestFrameSlot frameSlot_;

    bool connected_ = false;
    QString cameraName_;
    QString errorMessage_;
    std::atomic<std::uint64_t> frameCount_{0};
    std::chrono::steady_clock::time_point startTime_;  // GUI thread only (set in rescan())

    // Touched only from frame(), which callers must only invoke from the
    // render/GUI thread, so these are not atomic despite being mutable.
    mutable std::optional<std::uint64_t> lastRenderedSequence_;
    mutable std::atomic<std::uint64_t> renderedFrameCount_{0};
};

}  // namespace astro::app
