#pragma once
#include "MaskSettings.hpp"
#include <QQuickPaintedItem>
#include <QPainter>
#include <QTimer>
#include <QtQml/qqmlregistration.h>
namespace astro::app {
class MaskOverlayItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QVariantMap settings READ settings WRITE setSettings NOTIFY settingsChanged)
    Q_PROPERTY(QImage raster READ raster WRITE setRaster NOTIFY rasterChanged)
    Q_PROPERTY(int sourceWidth MEMBER sourceWidth_ NOTIFY dimensionsChanged)
    Q_PROPERTY(int sourceHeight MEMBER sourceHeight_ NOTIFY dimensionsChanged)
    Q_PROPERTY(bool maskOnly MEMBER maskOnly_ NOTIFY viewChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
public:
    explicit MaskOverlayItem(QQuickItem* parent=nullptr):QQuickPaintedItem(parent){timer_.setSingleShot(true);timer_.setInterval(30);connect(&timer_,&QTimer::timeout,this,&MaskOverlayItem::refresh);connect(this,&MaskOverlayItem::dimensionsChanged,this,[this]{timer_.start();});connect(this,&MaskOverlayItem::viewChanged,this,[this]{timer_.start();});}
    QVariantMap settings()const{return settings_;}void setSettings(const QVariantMap& s){settings_=s;emit settingsChanged();timer_.start();}
    QImage raster()const{return raster_;}void setRaster(const QImage& p){raster_=p;emit rasterChanged();timer_.start();}
    QString error()const{return error_;}
    void paint(QPainter* p)override{if(!preview_.isNull())p->drawImage(QRectF(0,0,width(),height()),preview_);}
signals:void settingsChanged();void rasterChanged();void dimensionsChanged();void viewChanged();void errorChanged();
private:
    void refresh(){
        if(sourceWidth_<1||sourceHeight_<1)return;
        try{
            const double scale=std::min(1.,768./std::max(sourceWidth_,sourceHeight_));const int w=std::max(1,int(std::lround(sourceWidth_*scale))),h=std::max(1,int(std::lround(sourceHeight_*scale)));
            processing::FloatImage dummy{w,h,std::vector<float>(std::size_t(w)*h*3)};auto options=maskOptions(settings_);options.blurRadius*=scale;
            if(options.type==5){if(raster_.isNull())throw std::runtime_error("Mask preview is unavailable.");const auto gray=raster_.scaled(w,h,Qt::IgnoreAspectRatio,Qt::SmoothTransformation).convertToFormat(QImage::Format_Grayscale8);options.raster.resize(std::size_t(w)*h);for(int y=0;y<h;++y)for(int x=0;x<w;++x)options.raster[std::size_t(y)*w+x]=gray.constScanLine(y)[x]/255.f;}
            const auto mask=processing::makeMask(dummy,options);preview_=QImage(w,h,QImage::Format_ARGB32_Premultiplied);
            for(int y=0;y<h;++y){auto* row=reinterpret_cast<QRgb*>(preview_.scanLine(y));for(int x=0;x<w;++x){const auto v=mask[std::size_t(y)*w+x];if(maskOnly_){const int c=int(std::lround(v*255));row[x]=qRgb(c,c,c);}else{const int alpha=int(std::lround((1-v)*130));row[x]=qRgba(alpha,int(alpha*.12),int(alpha*.08),alpha);}}}error_.clear();
        }catch(const std::exception& e){preview_={};error_=QString::fromUtf8(e.what());}emit errorChanged();update();
    }
    QVariantMap settings_;QImage raster_,preview_;QTimer timer_;QString error_;int sourceWidth_{},sourceHeight_{};bool maskOnly_{};
};
}
