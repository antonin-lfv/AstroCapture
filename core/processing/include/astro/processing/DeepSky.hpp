#pragma once
#include <astro/processing/Planetary.hpp>
#include <astro/recording/AstroImage.hpp>
#include <functional>
#include <optional>
namespace astro::processing {
using recording::AstroImage;
using ImageLoader = std::function<AstroImage(int)>;
using Progress = std::function<void(int,int,const std::string&)>;
struct Calibration {
    std::optional<AstroImage> dark, flat, bias, darkFlat, badPixels;
};
AstroImage masterFrame(int count, const ImageLoader&, bool normalize, const CheckCancel& = {});
AstroImage detectBadPixels(const AstroImage& dark,double sigma=8);
AstroImage calibrate(AstroImage light, const Calibration&, const CheckCancel& = {});
FloatImage linearRgb(const AstroImage&, const CheckCancel& = {});
struct Star { double x{},y{},flux{},fwhm{}; };
std::vector<Star> detectStars(const FloatImage&, const CheckCancel& = {});
struct StarAlignment {
    std::array<double,6> matrix{1,0,0,0,1,0}; // source -> reference
    int matches{};
    double residual{};
    double fluxScale{1};
};
StarAlignment alignStars(const FloatImage& reference,const FloatImage& source,const CheckCancel& = {});
struct Integration {
    FloatImage image;
    std::vector<int> accepted, rejected;
    std::vector<std::string> reasons;
    std::uint64_t rejectedSamples{};
    double totalExposure{};
};
Integration integrate(int count,const ImageLoader&,const Calibration&,bool calibrated,double sigma,
                      const CheckCancel& = {},const Progress& = {},bool normalize=true);
struct Finish {
    bool background{};
    bool stretch{true};
    float black{0},white{1},midtones{0.15f};
    std::array<float,3> color{1,1,1};
    float denoise{};
    bool asinh{};
    float asinhStrength{20},saturation{1};
};
FloatImage finishDeepSky(const FloatImage&,const Finish&,const CheckCancel& = {});
FloatImage combineChannels(const FloatImage& red,const FloatImage& green,const FloatImage& blue,
                          const FloatImage* luminance=nullptr,const CheckCancel& = {});
}
