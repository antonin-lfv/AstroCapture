#include "ProcessingImageItem.hpp"

#include <QPainter>
#include <algorithm>
#include <cmath>
namespace astro::app {
void ProcessingImageItem::setController(ProcessingController* controller) {
    if (controller_ == controller) return;
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = controller;
    if (controller_) {
        annotation_ = controller_->annotationImage();
        connect(controller_, &ProcessingController::annotationChanged, this, [this] {
            annotation_ = controller_->annotationImage();
            update();
        });
        connect(controller_, &ProcessingController::imageChanged, this, [this] {
            image_ = controller_->image();
            before_ = controller_->beforeImage();
            showingResult_ = controller_->showingResult();
            update();
        });
        image_ = controller_->image();
        before_ = controller_->beforeImage();
        showingResult_ = controller_->showingResult();
    } else
        image_ = {};
    emit controllerChanged();
    update();
}
void ProcessingImageItem::setSplit(double value) {
    if (!std::isfinite(value)) return;
    value = std::clamp(value, 0.0, 1.0);
    if (split_ == value) return;
    split_ = value;
    emit comparisonChanged();
    update();
}
void ProcessingImageItem::paint(QPainter* painter) {
    if (image_.isNull()) return;
    const QRectF bounds(0, 0, width(), height());
    const bool available = showingResult_ && !before_.isNull() && before_.size() == image_.size();
    painter->drawImage(bounds, showBefore_ && available ? before_ : image_);
    if (compare_ && available && !showBefore_) {
        painter->save();
        painter->setClipRect(QRectF(0, 0, width() * split_, height()));
        painter->drawImage(bounds, before_);
        painter->restore();
    }
    if (showingResult_ && !showBefore_ && !annotation_.isNull())
        painter->drawImage(bounds, annotation_);
}
}  // namespace astro::app
