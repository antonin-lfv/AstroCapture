#pragma once
#include "GeometrySettings.hpp"
#include <astro/processing/Geometry.hpp>
#include <QImage>
#include <QTransform>
#include <algorithm>
#include <stdexcept>
namespace astro::app {
inline processing::Geometry geometryFrom(const QVariantMap& v){return {v["crop"].toBool(),v["x"].toInt(),v["y"].toInt(),v["width"].toInt(),v["height"].toInt(),v["angle"].toDouble(),v["scale"].toDouble(),v["interpolation"].toInt(),v["clamp"].toBool(),v["keepSize"].toBool()};}
inline QImage imageGeometry(const QImage& image,const QVariantMap& values){
    if(!geometryActive(values)||image.isNull())return image;
    const auto rgb=image.convertToFormat(QImage::Format_RGB888);processing::FloatImage f{rgb.width(),rgb.height(),std::vector<float>(std::size_t(rgb.width())*rgb.height()*3)};
    for(int y=0;y<rgb.height();++y){const auto* row=rgb.constScanLine(y);for(int x=0;x<rgb.width()*3;++x)f.rgb[std::size_t(y)*rgb.width()*3+x]=row[x]/255.f;}
    f=processing::transformGeometry(f,geometryFrom(values));QImage out(f.width,f.height,QImage::Format_RGB888);if(out.isNull())throw std::runtime_error("Cannot allocate geometry preview.");
    for(int y=0;y<f.height;++y){auto* row=out.scanLine(y);for(int x=0;x<f.width*3;++x)row[x]=uchar(std::lround(std::clamp(f.rgb[std::size_t(y)*f.width*3+x],0.f,1.f)*255));}return out;
}
}
