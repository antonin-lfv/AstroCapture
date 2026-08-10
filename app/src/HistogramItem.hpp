#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

#include "PreviewController.hpp"

namespace astro::app {

// Paints the controller's latest-frame histogram as a filled curve: a
// single luminance curve for raw/mono sources, or overlaid R/G/B curves for
// formats with separable color channels. Mirrors LivePreviewItem's
// controller-bound QQuickPaintedItem pattern.
class HistogramItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::PreviewController* controller READ controller WRITE setController NOTIFY
                   controllerChanged)

public:
    explicit HistogramItem(QQuickItem* parent = nullptr);

    PreviewController* controller() const { return controller_; }
    void setController(PreviewController* controller);

    void paint(QPainter* painter) override;

signals:
    void controllerChanged();

private:
    QPointer<PreviewController> controller_;
};

}  // namespace astro::app
