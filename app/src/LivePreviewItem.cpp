#include "LivePreviewItem.hpp"

#include <QPainter>

namespace astro::app {

LivePreviewItem::LivePreviewItem(QQuickItem* parent) : QQuickPaintedItem(parent) {}

void LivePreviewItem::setController(PreviewController* controller) {
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

void LivePreviewItem::paint(QPainter* painter) {
    if (!controller_) {
        return;
    }
    const QImage image = controller_->frame();
    if (image.isNull()) {
        return;
    }

    const QSizeF target(width(), height());
    const QSizeF scaled = image.size().scaled(target.toSize(), Qt::KeepAspectRatio);
    const QRectF centered(QPointF((target.width() - scaled.width()) / 2.0, (target.height() - scaled.height()) / 2.0),
                           scaled);
    painter->drawImage(centered, image);

    if (imageRect_ != centered) {
        imageRect_ = centered;
        emit imageRectChanged();
    }
}

}  // namespace astro::app
