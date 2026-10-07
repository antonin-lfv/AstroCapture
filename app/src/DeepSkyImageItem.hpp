#pragma once
#include "DeepSkyController.hpp"
#include <QPointer>
#include <QQuickPaintedItem>
#include <QPainter>
#include <QPolygonF>
#include <array>
#include <algorithm>
#include <cmath>
namespace astro::app {
class DeepSkyImageItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::DeepSkyController* controller READ controller WRITE setController NOTIFY controllerChanged)
public:
    explicit DeepSkyImageItem(QQuickItem* parent=nullptr):QQuickPaintedItem(parent){}
    DeepSkyController* controller()const{return controller_;}
    void setController(DeepSkyController* value){if(controller_==value)return;if(controller_)disconnect(controller_,nullptr,this,nullptr);controller_=value;image_=value?value->image():QImage{};if(value)connect(value,&DeepSkyController::imageChanged,this,[this]{image_=controller_->image();update();});emit controllerChanged();update();}
    void paint(QPainter* painter)override{if(!image_.isNull())painter->drawImage(QRectF(0,0,width(),height()),image_);}
signals:
    void controllerChanged();
private:
    QPointer<DeepSkyController> controller_;QImage image_;
};
}

namespace astro::app {
class DeepSkyHistogramItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(astro::app::DeepSkyController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(bool luminanceOnly READ luminanceOnly WRITE setLuminanceOnly NOTIFY modeChanged)
public:
    explicit DeepSkyHistogramItem(QQuickItem* parent=nullptr):QQuickPaintedItem(parent){}
    DeepSkyController* controller()const{return controller_;}
    bool luminanceOnly()const{return luminanceOnly_;}
    void setLuminanceOnly(bool value){luminanceOnly_=value;emit modeChanged();update();}
    void setController(DeepSkyController* value) {
        if(controller_==value)return;if(controller_)disconnect(controller_,nullptr,this,nullptr);controller_=value;
        if(value)connect(value,&DeepSkyController::imageChanged,this,[this]{refresh();});refresh();emit controllerChanged();
    }
    void paint(QPainter* painter)override {
        painter->fillRect(QRectF(0,0,width(),height()),QColor("#12161c"));
        const std::array<QColor,4> colors{QColor(255,100,100,150),QColor(100,255,130,150),QColor(110,160,255,150),QColor(210,220,240,180)};
        const int start=luminanceOnly_?3:0,end=luminanceOnly_?4:3;
        for(int c=start;c<end;++c){const auto maximum=*std::max_element(bins_[c].begin(),bins_[c].end());if(!maximum)continue;QPolygonF polygon;polygon<<QPointF(0,height());for(int i=0;i<256;++i)polygon<<QPointF(i*width()/255,height()*(1-std::log1p(bins_[c][i])/std::log1p(maximum)));polygon<<QPointF(width(),height());painter->setPen(Qt::NoPen);painter->setBrush(colors[c]);painter->drawPolygon(polygon);}
    }
signals:
    void controllerChanged();void modeChanged();
private:
    void refresh(){for(auto& channel:bins_)channel.fill(0);if(controller_){const auto image=controller_->image().convertToFormat(QImage::Format_RGB888);const int step=std::max(1,int(std::sqrt(double(image.width())*image.height()/200000.)));for(int y=0;y<image.height();y+=step){const auto* row=image.constScanLine(y);for(int x=0;x<image.width();x+=step){for(int c=0;c<3;++c)++bins_[c][row[x*3+c]];++bins_[3][int(.2126*row[x*3]+.7152*row[x*3+1]+.0722*row[x*3+2])];}}}update();}
    QPointer<DeepSkyController> controller_;std::array<std::array<unsigned,256>,4> bins_{};bool luminanceOnly_{};
};
}
