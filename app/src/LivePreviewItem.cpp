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
        // previewTick(), not frameChanged(): repainting on every acquired
        // frame (100+/sec) made window/slider dragging visibly janky by
        // re-running debayer on the GUI thread that often. previewTick is
        // deliberately rate-capped; frame() always returns whatever is
        // newest regardless, so this only throttles how often we ask.
        connect(controller_, &PreviewController::previewTick, this, [this] { update(); });
    }
    emit controllerChanged();
    update();
}

void LivePreviewItem::paint(QPainter* painter) {
    if (!controller_) {
        return;
    }
    const QImage image = controller_->previewImage();
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
