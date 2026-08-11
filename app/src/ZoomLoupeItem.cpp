#include "ZoomLoupeItem.hpp"

#include <algorithm>

#include <QPainter>
#include <QPen>

namespace astro::app {

ZoomLoupeItem::ZoomLoupeItem(QQuickItem* parent) : QQuickPaintedItem(parent) {}

void ZoomLoupeItem::setController(PreviewController* controller) {
    if (controller_ == controller) {
        return;
    }
    if (controller_) {
        disconnect(controller_, nullptr, this, nullptr);
    }
    controller_ = controller;
    if (controller_) {
        connect(controller_, &PreviewController::frameChanged, this, [this] {
            if (active_) {
                update();
            }
        });
    }
    emit controllerChanged();
    update();
}

void ZoomLoupeItem::setSourceX(qreal value) {
    if (qFuzzyCompare(sourceX_, value)) {
        return;
    }
    sourceX_ = value;
    emit sourceChanged();
    if (active_) {
        update();
    }
}

void ZoomLoupeItem::setSourceY(qreal value) {
    if (qFuzzyCompare(sourceY_, value)) {
        return;
    }
    sourceY_ = value;
    emit sourceChanged();
    if (active_) {
        update();
    }
}

void ZoomLoupeItem::setZoomFactor(int value) {
    value = std::max(1, value);
    if (zoomFactor_ == value) {
        return;
    }
    zoomFactor_ = value;
    emit zoomFactorChanged();
    if (active_) {
        update();
    }
}

void ZoomLoupeItem::setActive(bool value) {
    if (active_ == value) {
        return;
    }
    active_ = value;
    emit activeChanged();
    update();
}

void ZoomLoupeItem::paint(QPainter* painter) {
    const QRectF area(0, 0, width(), height());
    painter->fillRect(area, QColor("#0b0d10"));

    if (!controller_ || !active_) {
        return;
    }

    const QImage image = controller_->frame();
    if (image.isNull() || area.width() <= 0 || area.height() <= 0) {
        return;
    }

    const int srcX = std::clamp(static_cast<int>(sourceX_ * image.width()), 0, image.width() - 1);
    const int srcY = std::clamp(static_cast<int>(sourceY_ * image.height()), 0, image.height() - 1);

    const int halfPatchW = std::max(1, static_cast<int>(area.width() / (2.0 * zoomFactor_)));
    const int halfPatchH = std::max(1, static_cast<int>(area.height() / (2.0 * zoomFactor_)));
    const QRect patch(srcX - halfPatchW, srcY - halfPatchH, halfPatchW * 2, halfPatchH * 2);
    const QRect clampedPatch = patch.intersected(QRect(0, 0, image.width(), image.height()));
    if (clampedPatch.isEmpty()) {
        return;
    }

    // Nearest-neighbor on purpose: this is for inspecting individual
    // pixels/focus, not for a pretty smoothed magnification.
    painter->setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter->drawImage(area, image.copy(clampedPatch));

    const QColor crosshairColor(255, 255, 255, 160);
    painter->setPen(QPen(crosshairColor, 1));
    painter->drawLine(QPointF(area.center().x(), area.top()), QPointF(area.center().x(), area.bottom()));
    painter->drawLine(QPointF(area.left(), area.center().y()), QPointF(area.right(), area.center().y()));
}

}  // namespace astro::app
