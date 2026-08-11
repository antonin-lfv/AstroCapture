#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

#include "PreviewController.hpp"

namespace astro::app {

// Minimal QQuickPaintedItem bridge from PreviewController's QImage to the
// scene graph. A texture-based path (IMPLEMENTATION_BLUEPRINT.md section 10)
// can replace this once profiling shows QPainter-based repaint is a
// bottleneck; this exists to get correctness first.
class LivePreviewItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::PreviewController* controller READ controller WRITE setController NOTIFY
                   controllerChanged)
    // The frame's actual on-screen rect within this item, in this item's own
    // local coordinates (i.e. (0,0) is this item's top-left) — the
    // KeepAspectRatio-fitted, letterboxed rect paint() draws into. Overlays
    // (crosshair, zoom loupe) need this to align with the image rather than
    // the item's full (possibly letterboxed) bounds. Updated from paint(),
    // which Qt Quick's threaded render loop may call on the render thread;
    // the NOTIFY signal crossing to QML's GUI-thread bindings is safe
    // because Qt signal emission is thread-safe regardless of caller thread
    // (see PreviewController::onFrame's same reasoning).
    Q_PROPERTY(QRectF imageRect READ imageRect NOTIFY imageRectChanged)

public:
    explicit LivePreviewItem(QQuickItem* parent = nullptr);

    PreviewController* controller() const { return controller_; }
    void setController(PreviewController* controller);

    QRectF imageRect() const { return imageRect_; }

    void paint(QPainter* painter) override;

signals:
    void controllerChanged();
    void imageRectChanged();

private:
    QPointer<PreviewController> controller_;
    QRectF imageRect_;
};

}  // namespace astro::app
