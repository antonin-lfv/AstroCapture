#include <astro/processing/DeepSky.hpp>
#include <astro/processing/Preview.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <future>
#include <limits>
#include <random>
#include "Check.hpp"
using namespace astro::processing;
FloatImage fixture(int width,int height) {
    FloatImage image{width,height,std::vector<float>(std::size_t(width)*height*3)};
    std::mt19937 generator(123);std::uniform_real_distribution<float> noise(-.01f,.01f);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x)for(int c=0;c<3;++c)image.rgb[(std::size_t(y)*width+x)*3+c]=noise(generator)+.03f+.25f*std::exp(-float((x-width/2)*(x-width/2)+(y-height/2)*(y-height/2))/25.f)+float(x)/std::max(1,width)*.06f+float(c)*.02f;
    // Exercise corner/border support and values outside the normalized range.
    image.rgb[0]=1.7f;image.rgb.back()=-.03f;return image;
}
void equal(const FloatImage& cpu,const FloatImage& gpu,float absolute=3e-6f,float relative=1e-5f) {
    ASTRO_TEST_CHECK(cpu.width==gpu.width&&cpu.height==gpu.height&&cpu.rgb.size()==gpu.rgb.size());
    float maximum=0;
    for(std::size_t i=0;i<cpu.rgb.size();++i){const float difference=std::abs(cpu.rgb[i]-gpu.rgb[i]);maximum=std::max(maximum,difference);if(difference>absolute+relative*std::abs(cpu.rgb[i]))std::cerr<<"Precision failure at "<<i<<": CPU "<<cpu.rgb[i]<<", Metal "<<gpu.rgb[i]<<"\n";ASTRO_TEST_CHECK(std::isfinite(gpu.rgb[i])&&difference<=absolute+relative*std::abs(cpu.rgb[i]));}
}
void disable(bool value){
#ifdef _WIN32
    _putenv_s("ASTRO_DISABLE_METAL",value?"1":"0");
#else
    if(value)setenv("ASTRO_DISABLE_METAL","1",1);else unsetenv("ASTRO_DISABLE_METAL");
#endif
}
int main(){
    const bool wasDisabled=std::getenv("ASTRO_DISABLE_METAL")&&std::string(std::getenv("ASTRO_DISABLE_METAL"))=="1";
    const auto availability=metalAvailability();const bool gpu=availability.available;
    std::cout<<"Metal: "<<(gpu?availability.device:availability.reason)<<"\n";
    auto image=fixture(129,97);const auto untouched=image.rgb;
    Finish options;ComputeReport report;
    // Force the CPU oracle and the requested backend, including odd dimensions,
    // clamping, un-stretched negative values, colors, asinh and Gaussian borders.
    for(bool stretch:{false,true})for(bool asinh:{false,true})for(float smoothing:{0.f,.4f,1.f})for(float saturation:{0.f,1.f,2.7f}) {
        options.stretch=stretch;options.asinh=asinh;options.denoise=smoothing;options.saturation=saturation;options.color={.18f,.7f,1.2f};options.black=.015;options.white=.32;options.midtones=.017;options.asinhStrength=150;
        options.compute=ComputePreference::Cpu;const auto cpu=finishDeepSky(image,options);
        options.compute=ComputePreference::Metal;const auto metal=finishDeepSky(image,options,{},&report);
        ASTRO_TEST_CHECK(report.backend==(gpu?ComputeBackend::Metal:ComputeBackend::Cpu));equal(cpu,metal);
    }
    for(int luminance=0;luminance<3;++luminance){options.asinhLuminance=luminance;options.asinh=true;options.asinhStrength=1000;options.compute=ComputePreference::Cpu;const auto cpu=finishDeepSky(image,options);options.compute=ComputePreference::Metal;equal(cpu,finishDeepSky(image,options));}options.asinhLuminance=0;options.asinhStrength=20;
    options.background=true;options.compute=ComputePreference::Cpu;const auto gradient=finishDeepSky(image,options);options.compute=ComputePreference::Metal;equal(gradient,finishDeepSky(image,options,{},&report));
    for(const auto size:std::array<std::array<int,2>,4>{{{1,1},{1,9},{9,1},{3,5}}}) {
        auto tiny=fixture(size[0],size[1]);options.background=false;options.compute=ComputePreference::Cpu;const auto cpu=finishDeepSky(tiny,options);options.compute=ComputePreference::Metal;equal(cpu,finishDeepSky(tiny,options));
    }
    const std::array<float,6> gains{2,1,.6f,.3f,.2f,.1f};
    auto strips=fixture(113,1301);options.compute=ComputePreference::Cpu;options.denoise=.7;options.asinh=true;
    const auto stripFinish=finishDeepSky(strips,options);options.compute=ComputePreference::Metal;equal(stripFinish,finishDeepSky(strips,options));
    equal(wavelets(strips,gains,0,{1.2f,.8f,1},.8f,{},ComputePreference::Cpu),wavelets(strips,gains,0,{1.2f,.8f,1},.8f,{},ComputePreference::Metal),3e-5f,2e-5f);
    for(float gamma:{.1f,.8f,1.f,2.2f,5.f})for(float denoise:{0.f,.001f,.5f}) {
        auto cpu=wavelets(image,gains,denoise,{1.2f,.8f,.6f},gamma,{},ComputePreference::Cpu);
        auto metal=wavelets(image,gains,denoise,{1.2f,.8f,.6f},gamma,{},ComputePreference::Metal,&report);
        ASTRO_TEST_CHECK(report.backend==(gpu?ComputeBackend::Metal:ComputeBackend::Cpu));equal(cpu,metal,3e-5f,2e-5f);
    }
    // Exact RGB8 rounding, clipping and caller-owned row padding. Include values
    // immediately around every quantization midpoint, not just random samples.
    FloatImage thresholds{257,3,std::vector<float>(257*3*3)};
    for(int n=0;n<257;++n)for(int side=0;side<3;++side)for(int c=0;c<3;++c){const float value=(n+.5f)/255.f;thresholds.rgb[(side*257+n)*3+c]=side==0?std::nextafter(value,0.f):side==1?value:std::nextafter(value,2.f);}
    for(const auto* source:{&image,&strips,&thresholds}){
        const auto stride=std::size_t(source->width)*3+7;std::vector<std::uint8_t> cpu(stride*source->height,97),metal=cpu;
        renderRgb8(*source,cpu,stride,{},ComputePreference::Cpu);renderRgb8(*source,metal,stride,{},ComputePreference::Metal,&report);
        ASTRO_TEST_CHECK(report.backend==(gpu?ComputeBackend::Metal:ComputeBackend::Cpu)&&cpu==metal);
    }
    for(const auto* source:{&image,&strips}){
        const auto stride=std::size_t(source->width)*3+7;std::vector<std::uint8_t> pixels(stride*source->height,71),expectedPixels=pixels;
        const PreviewTarget target{pixels,stride};
        options.compute=ComputePreference::Cpu;const auto cpu=finishDeepSky(*source,options,{},&report,&target);renderRgb8(cpu,expectedPixels,stride,{},ComputePreference::Cpu);ASTRO_TEST_CHECK(pixels==expectedPixels);
        options.compute=ComputePreference::Metal;std::fill(pixels.begin(),pixels.end(),71);std::fill(expectedPixels.begin(),expectedPixels.end(),71);const auto metal=finishDeepSky(*source,options,{},&report,&target);renderRgb8(metal,expectedPixels,stride,{},ComputePreference::Cpu);ASTRO_TEST_CHECK(pixels==expectedPixels);equal(cpu,metal);
        std::fill(pixels.begin(),pixels.end(),71);const auto wave=wavelets(*source,gains,0,{1,1,1},.8f,{},ComputePreference::Metal,&report,&target);renderRgb8(wave,expectedPixels,stride,{},ComputePreference::Cpu);ASTRO_TEST_CHECK(pixels==expectedPixels);
    }
    bool badDestination=false;std::vector<std::uint8_t> tooSmall(3);try{renderRgb8(image,tooSmall,3);}catch(const std::runtime_error&){badDestination=true;}ASTRO_TEST_CHECK(badDestination);
    auto unchanged=wavelets(image,{},0,{1,1,1},1,{},ComputePreference::Metal);const auto expected=wavelets(image,{},0,{1,1,1},1,{},ComputePreference::Cpu);equal(expected,unchanged);ASTRO_TEST_CHECK(image.rgb==untouched);
    // Independent jobs share pipelines/queue, never scratch buffers or settings.
    auto one=std::async(std::launch::async,[&]{return wavelets(image,gains,0,{1,1,1},1,{},ComputePreference::Metal);});
    auto two=std::async(std::launch::async,[&, options]{return finishDeepSky(image,options);});
    equal(wavelets(image,gains,0,{1,1,1},1,{},ComputePreference::Cpu),one.get(),3e-5f,2e-5f);options.compute=ComputePreference::Cpu;equal(finishDeepSky(image,options),two.get());
    // Explicitly unavailable backend must fall back to CPU without changing pixels.
    disable(true);options.compute=ComputePreference::Metal;const auto fallback=finishDeepSky(image,options,{},&report);ASTRO_TEST_CHECK(report.backend==ComputeBackend::Cpu&&!report.fallbackReason.empty());options.compute=ComputePreference::Cpu;equal(fallback,finishDeepSky(image,options));
    const auto fallbackWavelets=wavelets(image,gains,0,{1,1,1},1,{},ComputePreference::Metal,&report);ASTRO_TEST_CHECK(report.backend==ComputeBackend::Cpu&&!report.fallbackReason.empty());equal(fallbackWavelets,wavelets(image,gains,0,{1,1,1},1,{},ComputePreference::Cpu));disable(wasDisabled);
    options.compute=ComputePreference::Auto;finishDeepSky(image,options,{},&report);ASTRO_TEST_CHECK(report.backend==ComputeBackend::Cpu);
    auto large=fixture(1024,1024);finishDeepSky(large,options,{},&report);ASTRO_TEST_CHECK(report.backend==(gpu?ComputeBackend::Metal:ComputeBackend::Cpu));
    auto planetarySize=fixture(512,512);wavelets(planetarySize,gains,0,{1,1,1},1,{},ComputePreference::Auto,&report);ASTRO_TEST_CHECK(report.backend==(gpu?ComputeBackend::Metal:ComputeBackend::Cpu));
    std::vector<std::uint8_t> automaticPreview(large.rgb.size());renderRgb8(large,automaticPreview,std::size_t(large.width)*3,{},ComputePreference::Auto,&report);ASTRO_TEST_CHECK(report.backend==ComputeBackend::Cpu);
    // Cancellation propagates as the original exception and never mutates input.
    int checkpoints=0;bool cancelled=false;
    try{wavelets(large,gains,0,{1,1,1},1,[&]{if(++checkpoints==60)throw std::logic_error("cancel");},ComputePreference::Metal);}catch(const std::logic_error&){cancelled=true;}ASTRO_TEST_CHECK(cancelled);
    if(gpu){
        ComputeReport inFlight;cancelled=false;bool observedPending=false;
        try{wavelets(large,gains,0,{1,1,1},1,[&]{if(inFlight.submittedCommands>inFlight.finishedCommands){observedPending=true;throw std::logic_error("cancel in flight");}},ComputePreference::Metal,&inFlight);}catch(const std::logic_error&){cancelled=true;}
        ASTRO_TEST_CHECK(cancelled&&observedPending&&inFlight.submittedCommands>0&&inFlight.submittedCommands==inFlight.finishedCommands);
    }
    options.compute=ComputePreference::Metal;cancelled=false;try{finishDeepSky(large,options,[]{throw std::logic_error("cancel");});}catch(const std::logic_error&){cancelled=true;}ASTRO_TEST_CHECK(cancelled);
    auto nonfinite=image;nonfinite.rgb[7]=std::numeric_limits<float>::quiet_NaN();options.stretch=false;options.denoise=0;options.saturation=1;finishDeepSky(nonfinite,options,{},&report);ASTRO_TEST_CHECK(report.backend==ComputeBackend::Cpu&&!report.fallbackReason.empty());
    bool rejected=false;options.midtones=std::numeric_limits<float>::quiet_NaN();try{finishDeepSky(image,options);}catch(const std::runtime_error&){rejected=true;}ASTRO_TEST_CHECK(rejected);
    rejected=false;try{finishDeepSky({},Finish{});}catch(const std::runtime_error&){rejected=true;}ASTRO_TEST_CHECK(rejected);
    std::cout<<"OK\n";
}
