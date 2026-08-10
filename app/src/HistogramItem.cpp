#include "HistogramItem.hpp"

#include <algorithm>

#include <QPainter>
#include <QPolygonF>

namespace astro::app {

namespace {

void paintCurve(QPainter* painter, const std::array<std::uint32_t, astro::processing::Histogram::kBucketCount>& buckets,
                 const QRectF& area, const QColor& color) {
    const auto maxCount = *std::max_element(buckets.begin(), buckets.end());
    if (maxCount == 0) {
        return;
    }

    QPolygonF polygon;
    const double bucketWidth = area.width() / static_cast<double>(buckets.size());
    for (std::size_t i = 0; i < buckets.size(); ++i) {
        const double fraction = static_cast<double>(buckets[i]) / static_cast<double>(maxCount);
        const double x = area.left() + static_cast<double>(i) * bucketWidth;
        const double y = area.bottom() - fraction * area.height();
        polygon << QPointF(x, y);
    }
    // Close along the bottom so the filled polygon reads as an area, not a wire.
    polygon << QPointF(area.right(), area.bottom()) << QPointF(area.left(), area.bottom());

    painter->setPen(Qt::NoPen);
    painter->setBrush(QBrush(color));
    painter->drawPolygon(polygon);
}

}  // namespace

HistogramItem::HistogramItem(QQuickItem* parent) : QQuickPaintedItem(parent) {}

void HistogramItem::setController(PreviewController* controller) {
    if (controller_ == controller) {
        return;
    }
    if (controller_) {
        disconnect(controller_, nullptr, this, nullptr);
    }
    controller_ = controller;
    if (controller_) {
        connect(controller_, &PreviewController::frameChanged, this, [this] { update(); });
    }
    emit controllerChanged();
    update();
}

void HistogramItem::paint(QPainter* painter) {
    painter->setRenderHint(QPainter::Antialiasing, false);
    const QRectF area(0, 0, width(), height());
    painter->fillRect(area, QColor("#12151a"));

    if (!controller_) {
        return;
    }

    const auto histogram = controller_->histogram();
    if (histogram.hasColorChannels) {
        painter->setCompositionMode(QPainter::CompositionMode_Plus);
        paintCurve(painter, histogram.red, area, QColor(255, 80, 80, 160));
        paintCurve(painter, histogram.green, area, QColor(80, 255, 80, 160));
        paintCurve(painter, histogram.blue, area, QColor(80, 140, 255, 160));
        painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    } else {
        paintCurve(painter, histogram.luminance, area, QColor(220, 220, 220, 200));
    }
}

}  // namespace astro::app
