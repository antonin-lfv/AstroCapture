#include "HistogramItem.hpp"

#include <algorithm>
#include <numeric>

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

// Fraction of all pixels sitting at the extreme bucket (0 or 255) above
// which the edge reads as "clipped" (crushed blacks / blown highlights)
// rather than just a normal dark/bright frame having a few pixels there.
constexpr double kClippingThreshold = 0.005;

void paintClippingIndicator(QPainter* painter, std::uint32_t extremeCount, std::uint32_t totalCount,
                             const QRectF& area, bool atRightEdge) {
    if (totalCount == 0) {
        return;
    }
    const double fraction = static_cast<double>(extremeCount) / static_cast<double>(totalCount);
    if (fraction < kClippingThreshold) {
        return;
    }

    constexpr double kBarWidth = 4.0;
    const double opacity = std::min(1.0, fraction / 0.05);  // fully opaque by 5% clipped
    const QRectF bar(atRightEdge ? area.right() - kBarWidth : area.left(), area.top(), kBarWidth, area.height());
    painter->fillRect(bar, QColor(229, 72, 77, static_cast<int>(120 + 135 * opacity)));
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

    const std::uint32_t totalPixels =
        std::accumulate(histogram.luminance.begin(), histogram.luminance.end(), std::uint32_t{0});
    paintClippingIndicator(painter, histogram.luminance.front(), totalPixels, area, /*atRightEdge=*/false);
    paintClippingIndicator(painter, histogram.luminance.back(), totalPixels, area, /*atRightEdge=*/true);
}

}  // namespace astro::app
