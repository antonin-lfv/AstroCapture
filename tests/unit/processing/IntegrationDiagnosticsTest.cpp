#include <astro/processing/DeepSky.hpp>
#include "Check.hpp"
#include <cmath>
#include <limits>
using namespace astro;
namespace {
recording::AstroImage scene() {
    recording::AstroImage a;a.width=a.height=96;a.channels=3;a.bits=32;a.scale=1;a.exposure=120;a.camera="test-camera";a.filter="RGB";a.samples.resize(96*96*3);
    const std::array<std::array<double,2>,10> stars{{{12,12},{36,10},{70,16},{14,40},{76,36},{60,65},{12,75},{36,70},{75,79},{33,40}}};
    for(int y=0;y<96;++y)for(int x=0;x<96;++x){double v=.01;for(const auto& p:stars)v+=.22*std::exp(-((x-p[0])*(x-p[0])+(y-p[1])*(y-p[1]))/5.);v+=3*std::exp(-((x-50)*(x-50)+(y-45)*(y-45))/40.);for(int c=0;c<3;++c)a.samples[(y*96+x)*3+c]=float(v*(1-c*.3));}
    for(int y=0;y<8;++y)for(int x=0;x<12;++x)for(int c=0;c<3;++c)a.samples[(y*96+x)*3+c]=x<8?-.02f:0;
    for(int y=85;y<89;++y)for(int x=85;x<89;++x)for(int c=0;c<3;++c)a.samples[(y*96+x)*3+c]=100;
    return a;
}
template<class F> void fails(F f){bool thrown=false;try{f();}catch(const std::exception&){thrown=true;}ASTRO_TEST_CHECK(thrown);}
}
int main() {
    auto truth=scene();std::vector<processing::HdrInput> inputs{{120,1},{30,1},{3,1}};
    auto load=[&](int n){auto a=truth;a.exposure=inputs[n].exposure;for(auto& v:a.samples)v=std::min(1.f,float(v*a.exposure/120));return a;};
    auto result=processing::mergeHdr(inputs,load);
    ASTRO_TEST_CHECK(result.recoveredPixels>50&&result.clippedPixels>=16);
    const auto center=(45*96+50)*3;
    for(int c=0;c<3;++c)ASTRO_TEST_CHECK(std::abs(result.image.rgb[center+c]-truth.samples[center+c])<.02);
    ASTRO_TEST_CHECK(result.image.rgb[center]>1&&result.recoveredMap.rgb[center]==1&&result.clippedMap.rgb[center]==0);
    ASTRO_TEST_CHECK(std::abs(result.image.rgb[center+1]/result.image.rgb[center]-.7)<.001);
    ASTRO_TEST_CHECK(std::abs(result.image.rgb[(3*96+3)*3]+.02)<1e-6&&std::abs(result.image.rgb[(3*96+10)*3])<1e-6);
    ASTRO_TEST_CHECK(std::abs(result.image.rgb[(86*96+86)*3]-40)<.01&&result.clippedMap.rgb[(86*96+86)*3]==1);
    // A source moved by exactly one pixel aligns to the reference rather than
    // blending displaced stars. The mask follows the clipped source samples.
    auto shifted=[&](int n){auto a=load(n);if(n==1){auto samples=a.samples;for(int y=0;y<96;++y)for(int x=1;x<96;++x)for(int c=0;c<3;++c)a.samples[(y*96+x)*3+c]=samples[(y*96+x-1)*3+c];}return a;};
    auto aligned=processing::mergeHdr(inputs,shifted);ASTRO_TEST_CHECK(aligned.alignments[1].matches>=6&&std::abs(aligned.alignments[1].matrix[2]+1)<.05);ASTRO_TEST_CHECK(std::abs(aligned.image.rgb[center]-truth.samples[center])<.03);
    fails([&]{processing::mergeHdr({{120,1},{120,1}},load);});
    fails([&]{processing::mergeHdr({{120,1},{0,1}},load);});
    fails([&]{processing::mergeHdr(inputs,load,0);});
    fails([&]{processing::mergeHdr(inputs,[&](int n){auto a=load(n);if(n)a.camera="other";return a;});});
    fails([&]{processing::mergeHdr(inputs,[&](int n){auto a=load(n);if(n)a.samples[10]=std::numeric_limits<float>::quiet_NaN();return a;});});
    fails([&]{processing::mergeHdr(inputs,[&](int n){auto a=load(n);if(n)a.samples.assign(a.samples.size(),.01f);return a;});});
    fails([&]{processing::mergeHdr(inputs,load,.1,[]{throw std::runtime_error("cancel");});});
    // Diagnostics must not change the integrated result. Counts are per color
    // after rejection, include both low/high outliers, and match global totals.
    auto lights=[&](int n){auto a=scene();for(auto& v:a.samples)v=std::min(v,.5f);if(n==3)a.samples[(60*96+40)*3]=10;return a;};
    auto plain=processing::integrate(4,lights,{},true,3,{}, {},false);
    auto maps=processing::integrate(4,lights,{},true,3,{}, {},false,nullptr,nullptr,true);
    processing::IntegrationLimits tiny;tiny.stripBytes=4*12*5;tiny.diskReserveBytes=0;
    const auto tiled=processing::integrate(4,lights,{},true,3,{}, {},false,nullptr,nullptr,true,false,&tiny);
    ASTRO_TEST_CHECK(tiled.image.rgb==maps.image.rgb && tiled.weights.rgb==maps.weights.rgb && tiled.contributors.rgb==maps.contributors.rgb && tiled.rejectionMap.rgb==maps.rejectionMap.rgb && tiled.rejectedSamples==maps.rejectedSamples);
    ASTRO_TEST_CHECK(plain.image.rgb==maps.image.rgb&&maps.weights.width==maps.image.width&&maps.rejectionMap.height==maps.image.height);
    double rejected=0;for(std::size_t i=0;i<maps.image.rgb.size();++i){ASTRO_TEST_CHECK(maps.weights.rgb[i]==maps.contributors.rgb[i]);ASTRO_TEST_CHECK(maps.contributors.rgb[i]+maps.rejectionMap.rgb[i]==4);rejected+=maps.rejectionMap.rgb[i];}
    ASTRO_TEST_CHECK(rejected==double(maps.rejectedSamples)&&rejected>0&&plain.weights.rgb.empty());
    processing::DrizzleOptions drizzle{1,1};auto projected=processing::integrate(4,lights,{},true,3,{}, {},false,nullptr,&drizzle,true);
    tiny.stripBytes=4*16*5;
    const auto tiledDrizzle=processing::integrate(4,lights,{},true,3,{}, {},false,nullptr,&drizzle,true,false,&tiny);
    ASTRO_TEST_CHECK(tiledDrizzle.image.rgb==projected.image.rgb && tiledDrizzle.weights.rgb==projected.weights.rgb && tiledDrizzle.rejectionMap.rgb==projected.rejectionMap.rgb);
    tiny.temporaryDirectory=std::filesystem::temp_directory_path()/"astrocapture-folder-that-does-not-exist";
    fails([&]{processing::integrate(4,lights,{},true,3,{}, {},false,nullptr,nullptr,true,false,&tiny);});
    tiny.temporaryDirectory.clear();tiny.diskReserveBytes=std::numeric_limits<std::uint64_t>::max();
    fails([&]{processing::integrate(4,lights,{},true,3,{}, {},false,nullptr,nullptr,true,false,&tiny);});
    for(std::size_t i=0;i<projected.image.rgb.size();++i)ASTRO_TEST_CHECK(projected.weights.rgb[i]>=0&&projected.weights.rgb[i]<=projected.contributors.rgb[i]+1e-4);
}
