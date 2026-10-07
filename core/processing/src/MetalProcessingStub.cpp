#include "MetalProcessing.hpp"
namespace astro::processing {
MetalAvailability metalAvailability(){return {false,{},"Metal is not enabled in this build."};}
namespace detail {
bool metalPreview(const FloatImage&,std::span<std::uint8_t>,std::size_t,const CheckCancel&,std::string& reason,ComputeReport*){reason=metalAvailability().reason;return false;}
bool metalFinish(const FloatImage&,const Finish&,FloatImage&,const CheckCancel&,std::string& reason,const PreviewTarget*,ComputeReport*){reason=metalAvailability().reason;return false;}
bool metalWavelets(const FloatImage&,const std::array<float,6>&,float,const std::array<float,3>&,float,FloatImage&,const CheckCancel&,std::string& reason,const PreviewTarget*,ComputeReport*){reason=metalAvailability().reason;return false;}
}
}
