#pragma once
#include <astro/processing/Preview.hpp>
#include <QImage>
#include <stdexcept>
namespace astro::app {
inline processing::PreviewTarget previewTarget(QImage& image) {return {{image.bits(),std::size_t(image.sizeInBytes())},std::size_t(image.bytesPerLine())};}
inline QImage allocateImagePreview(const processing::FloatImage& image) {
    QImage out(image.width,image.height,QImage::Format_RGB888);
    if(out.isNull())throw std::runtime_error("Not enough memory for image preview.");
    return out;
}
inline QImage makeImagePreview(const processing::FloatImage& image,const processing::CheckCancel& check={}) {
    auto out=allocateImagePreview(image);
    processing::renderRgb8(image,{out.bits(),std::size_t(out.sizeInBytes())},std::size_t(out.bytesPerLine()),check);
    return out;
}
}
