#pragma once
#include <astro/processing/DeepSky.hpp>
namespace astro::processing::detail {
bool metalPreview(const FloatImage&,std::span<std::uint8_t>,std::size_t,const CheckCancel&,std::string& reason,ComputeReport*);
bool metalFinish(const FloatImage&,const Finish&,FloatImage&,const CheckCancel&,std::string& reason,const PreviewTarget*,ComputeReport*);
bool metalWavelets(const FloatImage&,const std::array<float,6>&,float,
                   const std::array<float,3>&,float,FloatImage&,const CheckCancel&,std::string& reason,const PreviewTarget*,ComputeReport*);
inline bool requestMetal(std::size_t pixels,ComputePreference preference,ComputeReport* report,std::size_t minimumPixels=1024*1024) {
    if(report)*report={};
    return preference==ComputePreference::Metal || (preference==ComputePreference::Auto && pixels>=minimumPixels);
}
}
