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
    // Gate D backlog 22.4: min/max/mean of the latest frame, computed once
    // per paint() alongside the curve itself (not recomputed separately by
    // a QML-side reader) and exposed so the sidebar can show them as text.
    Q_PROPERTY(bool hasStats READ hasStats NOTIFY statsChanged)
    Q_PROPERTY(int minValue READ minValue NOTIFY statsChanged)
    Q_PROPERTY(int maxValue READ maxValue NOTIFY statsChanged)
    Q_PROPERTY(double meanValue READ meanValue NOTIFY statsChanged)

public:
    explicit HistogramItem(QQuickItem* parent = nullptr);

    PreviewController* controller() const { return controller_; }
    void setController(PreviewController* controller);

    bool hasStats() const { return hasStats_; }
    int minValue() const { return minValue_; }
    int maxValue() const { return maxValue_; }
    double meanValue() const { return meanValue_; }

    void paint(QPainter* painter) override;

signals:
    void controllerChanged();
    void statsChanged();

private:
    QPointer<PreviewController> controller_;
    bool hasStats_ = false;
    int minValue_ = 0;
    int maxValue_ = 0;
    double meanValue_ = 0.0;
};

}  // namespace astro::app
