#pragma once
#include <astro/recording/SerReader.hpp>
#include <astro/recording/RawSequenceReader.hpp>
#include <astro/recording/AstroImage.hpp>
#include <astro/processing/DeepSky.hpp>
#include <memory>
#include <algorithm>
#include <stdexcept>
namespace astro::app {
class SequenceReader {
public:
    explicit SequenceReader(const std::filesystem::path& path) {
        auto extension=path.extension().string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return std::tolower(c);});
        if(extension==".ser") {ser_=std::make_unique<recording::SerReader>(path);width=ser_->width;height=ser_->height;count=ser_->count;bits=ser_->bits;}
        else {raw_=std::make_unique<recording::RawSequenceReader>();auto result=raw_->open(path.string());if(!result)throw std::runtime_error(result.error().message);const auto h=result.value();width=h.descriptor.width;height=h.descriptor.height;count=int(h.frameCount);bits=h.descriptor.validBits;if(!count)throw std::runtime_error("ACRAW contains no frames.");}
    }
    processing::FloatImage image(int index) {
        if(ser_)return processing::decodeSamples(ser_->read(index),width,height,ser_->channels,bits,ser_->bayer);
        auto result=raw_->readFrame(index);if(!result)throw std::runtime_error(result.error().message);
        return processing::linearRgb(recording::imageFromFrame(result.value()));
    }
    int width{},height{},count{},bits{};
private:
    std::unique_ptr<recording::SerReader> ser_;
    std::unique_ptr<recording::RawSequenceReader> raw_;
};
}
