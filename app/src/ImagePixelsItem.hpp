#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <QPainter>
#include <QtQml/qqmlregistration.h>
namespace astro::app {
class ImagePixelsItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QImage image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(int sourceWidth READ sourceWidth NOTIFY imageChanged)
    Q_PROPERTY(int sourceHeight READ sourceHeight NOTIFY imageChanged)
public:
    using QQuickPaintedItem::QQuickPaintedItem;
    QImage image()const{return image_;}
    void setImage(const QImage& image){if(image.cacheKey()==image_.cacheKey())return;image_=image;emit imageChanged();update();}
    int sourceWidth()const{return image_.width();}int sourceHeight()const{return image_.height();}
    void paint(QPainter* p)override{if(!image_.isNull())p->drawImage(QRectF(0,0,width(),height()),image_);}
signals:void imageChanged();
private:QImage image_;
};
}
