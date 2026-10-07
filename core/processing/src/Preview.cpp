#include <astro/processing/Preview.hpp>
#include "MetalProcessing.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>
namespace astro::processing {
void validatePreviewTarget(const FloatImage& image,const PreviewTarget& target) {
    const auto destination=target.pixels;const auto stride=target.bytesPerLine;
    if(image.width<=0||image.height<=0||std::uint64_t(image.width)*image.height>32000000||image.rgb.size()!=std::size_t(image.width)*image.height*3)
        throw std::runtime_error("Invalid processing image dimensions.");
    const auto row=std::size_t(image.width)*3;
    if(!destination.data()||stride<row||stride>(std::numeric_limits<std::size_t>::max()-row)/std::size_t(image.height)||destination.size()<stride*std::size_t(image.height-1)+row)
        throw std::runtime_error("Invalid preview destination.");
    const auto sourceStart=reinterpret_cast<std::uintptr_t>(image.rgb.data()),sourceEnd=sourceStart+image.rgb.size()*sizeof(float);
    const auto destinationStart=reinterpret_cast<std::uintptr_t>(destination.data()),destinationEnd=destinationStart+stride*std::size_t(image.height-1)+row;
    if(destinationStart<sourceEnd&&sourceStart<destinationEnd)throw std::runtime_error("Preview destination overlaps scientific pixels.");
}
void renderRgb8(const FloatImage& image,std::span<std::uint8_t> destination,std::size_t stride,const CheckCancel& check,ComputePreference preference,ComputeReport* report) {
    validatePreviewTarget(image,{destination,stride});
    const auto row=std::size_t(image.width)*3;
    if(check)check();
    if(report)*report={};
    // Standalone conversion is faster on CPU than uploading a float image.
    // Automatic GPU previews are fused with finishing / wavelets instead.
    if(preference==ComputePreference::Metal){
        std::string reason;if(detail::metalPreview(image,destination,stride,check,reason,report)){if(report)report->backend=ComputeBackend::Metal;return;}if(report)report->fallbackReason=std::move(reason);
    }
    for(int y=0;y<image.height;++y){if(check)check();for(std::size_t x=0;x<row;++x){const float value=image.rgb[std::size_t(y)*row+x];destination[std::size_t(y)*stride+x]=std::isfinite(value)?std::uint8_t(std::lround(std::clamp(value,0.f,1.f)*255.f)):0;}}
}
}
