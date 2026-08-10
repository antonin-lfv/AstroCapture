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

public:
    explicit LivePreviewItem(QQuickItem* parent = nullptr);

    PreviewController* controller() const { return controller_; }
    void setController(PreviewController* controller);

    void paint(QPainter* painter) override;

signals:
    void controllerChanged();

private:
    QPointer<PreviewController> controller_;
};

}  // namespace astro::app
