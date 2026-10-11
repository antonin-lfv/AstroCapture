#include <astro/domain/ImageLimits.hpp>
#include <astro/processing/IntegrationResources.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
#endif

namespace astro::processing {
namespace {
std::uint64_t multiply(std::uint64_t a, std::uint64_t b) {
    if(b && a > std::numeric_limits<std::uint64_t>::max()/b)
        throw std::runtime_error("Integration resource estimate exceeds the supported range.");
    return a*b;
}
std::uint64_t add(std::uint64_t a, std::uint64_t b) {
    if(a > std::numeric_limits<std::uint64_t>::max()-b)
        throw std::runtime_error("Integration resource estimate exceeds the supported range.");
    return a+b;
}
}
int integrationStripColumns(int width, int count, int weightChannels, std::uint64_t budget) {
    if(width<1 || count<1 || count>500 || (weightChannels!=0 && weightChannels!=1 && weightChannels!=3))
        throw std::runtime_error("Invalid integration resource settings.");
    const auto pixel=std::uint64_t(count)*4ULL*(3+weightChannels);
    if(pixel>budget)throw std::runtime_error("The integration memory budget cannot hold one output pixel.");
    return int(std::min<std::uint64_t>(width,budget/pixel));
}
int integrationStripRows(int width, int count, int weightChannels, std::uint64_t budget) {
    if(integrationStripColumns(width,count,weightChannels,budget)!=width)
        throw std::runtime_error("One integration row exceeds the strip memory budget. Use narrower tiles.");
    const auto row=multiply(multiply(std::uint64_t(width),count),4ULL*(3+weightChannels));
    return int(std::max<std::uint64_t>(1,std::min<std::uint64_t>(65536ULL/(std::uint64_t(width)*3),budget/row)));
}
IntegrationResourceEstimate estimateIntegrationResources(const IntegrationResourceRequest& r,const IntegrationLimits& limits) {
    if(!astro::domain::supportedImageSize(r.width,r.height) ||
       (r.channels!=1 && r.channels!=3) || r.count<1 || r.count>500 ||
       !std::isfinite(r.scale) || (r.drizzle && (r.scale<1 || r.scale>2)) ||
       (r.drizzle && r.nativeBayer && r.channels!=1))
        throw std::runtime_error("Invalid integration resource settings.");
    IntegrationResourceEstimate e;
    const double scale=r.drizzle?r.scale:1;
    e.width=int(std::ceil(r.width*scale));e.height=int(std::ceil(r.height*scale));
    if(!astro::domain::supportedImageSize(e.width,e.height))
        throw std::runtime_error("The drizzle output exceeds 128 million pixels. Use a smaller scale or smaller source images.");
    const int weights=r.drizzle?(r.nativeBayer?3:1):0;
    e.stripColumns=integrationStripColumns(e.width,r.count,weights,limits.stripBytes);
    e.stripRows=std::min(e.height,integrationStripRows(e.stripColumns,r.count,weights,limits.stripBytes));
    const auto input=std::uint64_t(r.width)*r.height,output=std::uint64_t(e.width)*e.height;
    e.stripBytes=multiply(multiply(std::uint64_t(e.stripColumns)*e.stripRows,r.count),4ULL*(3+weights));
    e.temporaryBytes=multiply(multiply(output,r.count),4ULL*(3+weights));
    e.resultBytes=multiply(output,r.diagnosticMaps?48:12);
    // Reference/source/calibration/registration/background intermediates.
    // Drizzle double sums, weights and Float32 delivery now share a bounded
    // 64 MiB strip. Ordinary projection still has full-image intermediates.
    const auto source=multiply(input,4ULL*r.channels*4+12ULL*(r.sequenceBackground?10:8));
    const auto projection=r.drizzle?add(multiply(output,2),64ULL*1024*1024):multiply(output,38);
    // Result is counted separately: four RGB Float32-equivalent images cover
    // finishing/comparison/export preparation, plus native GPU/tile allowance.
    const auto display=add(multiply(output,48),256ULL*1024*1024);
    const auto rejection=add(add(multiply(input,12+4ULL*r.channels),output*2),add(e.resultBytes,e.stripBytes));
    e.workingBytes=add(std::max({add(source,projection),rejection,add(e.resultBytes,display)}),multiply(r.calibrationBytes,2));
    return e;
}
std::uint64_t physicalMemoryBytes() {return domain::physicalMemoryBytes();}
}
