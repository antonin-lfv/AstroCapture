#pragma once
#include "MosaicController.hpp"
#include <QPointer>
#include <QQuickPaintedItem>
#include <QPainter>
namespace astro::app {
class MosaicImageItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::MosaicController* controller READ controller WRITE setController NOTIFY controllerChanged)
public:
    explicit MosaicImageItem(QQuickItem* parent=nullptr):QQuickPaintedItem(parent){}
    MosaicController* controller() const {return controller_;}
    void setController(MosaicController* value) {
        if(controller_==value)return;
        if(controller_)disconnect(controller_,nullptr,this,nullptr);
        controller_=value; image_=value?value->image():QImage{};
        annotation_=value?value->annotationImage():QImage{};
        if(value)connect(value,&MosaicController::annotationChanged,this,[this]{annotation_=controller_->annotationImage();update();});
        if(value)connect(value,&MosaicController::imageChanged,this,[this]{image_=controller_->image();update();});
        emit controllerChanged();update();
    }
    void paint(QPainter* painter) override {if(!image_.isNull())painter->drawImage(QRectF(0,0,width(),height()),image_);if(!annotation_.isNull())painter->drawImage(QRectF(0,0,width(),height()),annotation_);}
signals:
    void controllerChanged();
private:
    QPointer<MosaicController> controller_;
    QImage image_, annotation_;
};
}
