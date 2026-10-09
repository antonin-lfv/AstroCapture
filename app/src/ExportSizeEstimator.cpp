#include "ExportSizeEstimator.hpp"
#include "ExportAnnotation.hpp"
#include <QBuffer>
#include <QColorSpace>
#include <QImageWriter>
#include <QMetaObject>
#include <algorithm>

namespace astro::app {
ExportSizeEstimator::ExportSizeEstimator(QObject* parent):QObject(parent) {
    timer_.setSingleShot(true);
    timer_.setInterval(250);
    connect(&timer_,&QTimer::timeout,this,&ExportSizeEstimator::update);
}
ExportSizeEstimator::~ExportSizeEstimator() {
    worker_.request_stop();
    if(worker_.joinable())worker_.join();
}
void ExportSizeEstimator::setInput(const QVariant& value) {
    const auto next=value.value<ExportSizeInput>();
    if(next.pixels==input_.pixels && next.overlay.cacheKey()==input_.overlay.cacheKey())return;
    input_=next;cache_.clear();invalidate();
}
void ExportSizeEstimator::setFormat(QString value) {
    value=value.toLower();
    if(value=="tiff")value="tif";
    if(value=="jpeg")value="jpg";
    if(format_==value)return;
    format_=std::move(value);invalidate();
}
void ExportSizeEstimator::setActive(bool value) {
    if(active_==value)return;
    active_=value;invalidate();
}
void ExportSizeEstimator::invalidate() {
    ++revision_;worker_.request_stop();timer_.stop();
    bytes_=-1;exact_=false;calculating_=active_ && bool(input_.pixels);
    emit changed();
    if(calculating_)timer_.start();
}
void ExportSizeEstimator::update() {
    if(!active_ || !input_.pixels)return;
    const auto& pixels=*input_.pixels;
    const auto count=std::uint64_t(pixels.width)*pixels.height;
    if(pixels.width<=0 || pixels.height<=0 || pixels.rgb.size()!=count*3) {
        calculating_=false;emit changed();return;
    }
    if(format_=="tif") {
        bytes_=double(count*6+192);exact_=true;calculating_=false;emit changed();return;
    }
    if(const auto found=cache_.find(format_);found!=cache_.end()) {
        bytes_=found->second.bytes;exact_=found->second.exact;calculating_=false;emit changed();return;
    }
    if(format_!="png" && format_!="jpg") {
        calculating_=false;emit changed();return;
    }
    if(running_)return; // Completion schedules the latest request.
    running_=true;
    const auto input=input_;const auto format=format_;const auto revision=revision_;
    worker_=std::jthread([this,input,format,revision](std::stop_token stop) {
        Estimate estimate{-1,false};
        try {
            const auto& source=*input.pixels;
            if(!input.overlay.isNull() && input.overlay.size()!=QSize(source.width,source.height))throw std::runtime_error("Invalid caption size.");
            const bool full=std::uint64_t(source.width)*source.height<=1024*1024;
            const int columns=full?1:std::min(8,std::max(1,source.width/128));
            const int rows=full?1:std::min(8,std::max(1,source.height/128));
            const int tileWidth=columns==1?source.width:std::min(128,source.width/columns);
            const int tileHeight=rows==1?source.height:std::min(128,source.height/rows);
            QImage sample(columns*tileWidth,rows*tileHeight,QImage::Format_RGBA64);
            if(sample.isNull())throw std::runtime_error("Cannot allocate size estimate.");
            sample.setColorSpace(QColorSpace::SRgb);
            for(int y=0;y<sample.height();++y) {
                if(stop.stop_requested())throw std::runtime_error("Cancelled");
                const int sourceY=rows==1?y:int(std::lround(double(y/tileHeight)*(source.height-tileHeight)/(rows-1)))+y%tileHeight;
                const auto* ink=input.overlay.isNull()?nullptr:reinterpret_cast<const QRgb*>(input.overlay.constScanLine(sourceY));
                auto* row=reinterpret_cast<QRgba64*>(sample.scanLine(y));
                for(int x=0;x<sample.width();++x) {
                    const int sourceX=columns==1?x:int(std::lround(double(x/tileWidth)*(source.width-tileWidth)/(columns-1)))+x%tileWidth;
                    const auto at=(std::size_t(sourceY)*source.width+sourceX)*3;
                    const auto overlay=ink?ink[sourceX]:QRgb(0);
                    row[x]=QRgba64::fromRgba64(annotatedSample(source.rgb[at],overlay,0),annotatedSample(source.rgb[at+1],overlay,1),annotatedSample(source.rgb[at+2],overlay,2),65535);
                }
            }
            if(format=="jpg")sample=sample.convertToFormat(QImage::Format_RGB888);
            QBuffer encoded;encoded.open(QIODevice::WriteOnly);
            QImageWriter writer(&encoded,format=="png"?"PNG":"JPEG");
            if(format=="jpg")writer.setQuality(95);
            if(!stop.stop_requested() && writer.write(sample)) {
                estimate={double(encoded.size())*source.width*source.height/(double(sample.width())*sample.height()),full};
            }
        } catch(const std::exception&) { /* Estimation failure must not prevent export. */ }
        QMetaObject::invokeMethod(this,[this,revision,format,estimate] {
            running_=false;
            if(revision!=revision_) {if(active_ && input_.pixels)timer_.start(0);return;}
            if(estimate.bytes>=0)cache_[format]=estimate;
            bytes_=estimate.bytes;exact_=estimate.exact;calculating_=false;emit changed();
        },Qt::QueuedConnection);
    });
}
}
