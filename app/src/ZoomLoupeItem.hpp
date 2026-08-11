#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

#include "PreviewController.hpp"

namespace astro::app {

// Paints a magnified, nearest-neighbor (unsmoothed — pixel peeping, not
// prettifying) crop of the controller's latest preview frame, centered on
// (sourceX, sourceY): normalized [0,1] coordinates within the frame. QML
// drives sourceX/sourceY/active from cursor position over LivePreviewItem's
// imageRect. Mirrors LivePreviewItem/HistogramItem's controller-bound
// QQuickPaintedItem pattern; independent of LivePreviewItem itself (calls
// controller->frame() directly), same as HistogramItem does for histogram().
class ZoomLoupeItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::PreviewController* controller READ controller WRITE setController NOTIFY
                   controllerChanged)
    Q_PROPERTY(qreal sourceX READ sourceX WRITE setSourceX NOTIFY sourceChanged)
    Q_PROPERTY(qreal sourceY READ sourceY WRITE setSourceY NOTIFY sourceChanged)
    Q_PROPERTY(int zoomFactor READ zoomFactor WRITE setZoomFactor NOTIFY zoomFactorChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit ZoomLoupeItem(QQuickItem* parent = nullptr);

    PreviewController* controller() const { return controller_; }
    void setController(PreviewController* controller);

    qreal sourceX() const { return sourceX_; }
    void setSourceX(qreal value);
    qreal sourceY() const { return sourceY_; }
    void setSourceY(qreal value);

    int zoomFactor() const { return zoomFactor_; }
    void setZoomFactor(int value);

    bool active() const { return active_; }
    void setActive(bool value);

    void paint(QPainter* painter) override;

signals:
    void controllerChanged();
    void sourceChanged();
    void zoomFactorChanged();
    void activeChanged();

private:
    QPointer<PreviewController> controller_;
    qreal sourceX_ = 0.5;
    qreal sourceY_ = 0.5;
    int zoomFactor_ = 6;
    bool active_ = false;
};

}  // namespace astro::app
