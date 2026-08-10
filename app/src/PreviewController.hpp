#pragma once

#include <atomic>
#include <cstdint>
#include <memory>

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

public:
    explicit PreviewController(QObject* parent = nullptr);
    ~PreviewController() override;

    bool connected() const { return connected_; }
    QString cameraName() const { return cameraName_; }
    QString errorMessage() const { return errorMessage_; }
    quint64 frameCount() const { return frameCount_.load(std::memory_order_relaxed); }

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
};

}  // namespace astro::app
