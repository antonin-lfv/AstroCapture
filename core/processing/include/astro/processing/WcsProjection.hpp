#pragma once
#include <astro/processing/DeepSky.hpp>

namespace astro::processing {
// Exact TAN plane homography plus forward/inverse SIP. Pixel centers are
// zero-based, matching CelestialWcs and the stored FITS row order.
class WcsProjection {
public:
    WcsProjection(const recording::CelestialWcs& source,const recording::CelestialWcs& target,
                  int width,int height,const CheckCancel& = {});
    std::array<double,2> forward(double x,double y) const;
    std::array<double,2> inverse(double x,double y) const;
private:
    recording::CelestialWcs source_,target_;
    std::array<double,9> forward_,inverse_;
};
// These relative checks do not certify the absolute catalogue solution.
StarAlignment validateWcsRegistration(const FloatImage& reference,const FloatImage& source,
                                     const WcsProjection&,const CheckCancel& = {});
FloatImage reprojectWcs(const FloatImage&,const WcsProjection&,int width,int height,const CheckCancel& = {});
} // namespace astro::processing
