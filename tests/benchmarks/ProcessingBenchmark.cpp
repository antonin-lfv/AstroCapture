#include <astro/processing/DeepSky.hpp>
#include <astro/recording/AstroImage.hpp>
#include <astro/processing/Preview.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace astro;
using Clock=std::chrono::steady_clock;
template<class F> double measure(F operation) {
    std::vector<double> times;
    for(int n=0;n<3;++n){const auto start=Clock::now();auto output=operation();times.push_back(std::chrono::duration<double,std::milli>(Clock::now()-start).count());(void)output;}
    std::sort(times.begin(),times.end());return times[1];
}
template<class F> void compare(const char* name,F operation,bool gpu) {
    processing::ComputeReport report;
    const double cpu=measure([&]{return operation(processing::ComputePreference::Cpu,nullptr);});
    std::cout<<name<<" · CPU "<<cpu<<" ms"<<std::flush;
    if(gpu){
        // Pipeline compilation is reported separately, before warm measurements.
        auto expected=operation(processing::ComputePreference::Cpu,nullptr);
        auto actual=operation(processing::ComputePreference::Metal,&report);
        if(report.backend!=processing::ComputeBackend::Metal)throw std::runtime_error(report.fallbackReason);
        float maximum=0;double squared=0;
        for(std::size_t i=0;i<expected.rgb.size();++i){const float difference=std::abs(expected.rgb[i]-actual.rgb[i]);maximum=std::max(maximum,difference);squared+=double(difference)*difference;if(!std::isfinite(actual.rgb[i])||difference>3e-5f+2e-5f*std::abs(expected.rgb[i]))throw std::runtime_error("CPU/Metal precision mismatch.");}
        // Do not retain outputs during timed runs: include all input/output copies.
        const auto samples=expected.rgb.size();expected={};actual={};
        const double metal=measure([&]{return operation(processing::ComputePreference::Metal,&report);});
        std::cout<<" · Metal "<<metal<<" ms · "<<cpu/metal<<"x · max error "<<std::scientific<<maximum<<" · RMS "<<std::sqrt(squared/samples)<<std::fixed;
    }
    std::cout<<"\n"<<std::flush;
}
int main(int argc,char** argv){try{
    if(argc<2||argc>4){std::cerr<<"Usage: astro_processing_benchmark linear.fits [--cpu] [--crop=N]\n";return 2;}
    bool cpuOnly=false;int crop=0;
    for(int i=2;i<argc;++i){const std::string arg=argv[i];if(arg=="--cpu")cpuOnly=true;else if(arg.starts_with("--crop="))crop=std::stoi(arg.substr(7));else throw std::runtime_error("Unknown benchmark option.");}
    auto raw=recording::readAstroImage(argv[1]);auto image=processing::linearRgb(raw);raw={};
    if(crop>0&&crop<image.width&&crop<image.height){processing::FloatImage cropped{crop,crop,std::vector<float>(std::size_t(crop)*crop*3)};for(int y=0;y<crop;++y)std::copy_n(image.rgb.begin()+std::size_t(y)*image.width*3,crop*3,cropped.rgb.begin()+std::size_t(y)*crop*3);image=std::move(cropped);}
    std::cout<<image.width<<"x"<<image.height<<" · RGB float32 · median of 3 runs, all copies included\n"<<std::fixed<<std::setprecision(3);
    const auto start=Clock::now();const auto info=processing::metalAvailability();
    std::cout<<"Metal: "<<info.device<<" · "<<(info.available?"available":info.reason)<<" · initialization "<<std::chrono::duration<double,std::milli>(Clock::now()-start).count()<<" ms\n"<<std::flush;
    const bool gpu=info.available&&!cpuOnly;
    auto preview=[&](processing::ComputePreference preference,processing::ComputeReport* report){std::vector<std::uint8_t> pixels(image.rgb.size());processing::renderRgb8(image,pixels,std::size_t(image.width)*3,{},preference,report);return pixels;};
    const double previewCpu=measure([&]{return preview(processing::ComputePreference::Cpu,nullptr);});
    std::cout<<"RGB8 preview · CPU "<<previewCpu<<" ms";
    if(gpu){processing::ComputeReport report;const auto cpu=preview(processing::ComputePreference::Cpu,nullptr),metal=preview(processing::ComputePreference::Metal,&report);if(report.backend!=processing::ComputeBackend::Metal||cpu!=metal)throw std::runtime_error("RGB8 preview mismatch.");const double time=measure([&]{return preview(processing::ComputePreference::Metal,&report);});std::cout<<" · Metal "<<time<<" ms · "<<previewCpu/time<<"x · exact bytes";}
    std::cout<<"\n"<<std::flush;
    processing::Finish settings;settings.black=.0004;settings.white=4.3;settings.midtones=.004;settings.color={.179f,.697f,1};settings.saturation=1.2;
    auto finish=[&](processing::ComputePreference preference,processing::ComputeReport* report){settings.compute=preference;return processing::finishDeepSky(image,settings,{},report);};
    compare("finish / midtones",finish,info.available&&!cpuOnly);
    compare("finish / midtones + RGB8 preview",[&](processing::ComputePreference preference,processing::ComputeReport* report){settings.compute=preference;std::vector<std::uint8_t> pixels(image.rgb.size());const processing::PreviewTarget target{pixels,std::size_t(image.width)*3};return processing::finishDeepSky(image,settings,{},report,&target);},info.available&&!cpuOnly);
    settings.asinh=true;settings.asinhStrength=100;compare("finish / asinh",finish,info.available&&!cpuOnly);
    compare("finish / asinh + RGB8 preview",[&](processing::ComputePreference preference,processing::ComputeReport* report){settings.compute=preference;std::vector<std::uint8_t> pixels(image.rgb.size());const processing::PreviewTarget target{pixels,std::size_t(image.width)*3};return processing::finishDeepSky(image,settings,{},report,&target);},info.available&&!cpuOnly);
    settings.denoise=.2;compare("finish / asinh + smoothing",finish,info.available&&!cpuOnly);
    settings.background=true;compare("finish / gradient + asinh + smoothing",finish,info.available&&!cpuOnly);
    compare("wavelets / 1 scale",[&](processing::ComputePreference preference,processing::ComputeReport* report){return processing::wavelets(image,{.5,0,0,0,0,0},.001,{1,1,1},1,{},preference,report);},info.available&&!cpuOnly);
    compare("wavelets / 6 scales",[&](processing::ComputePreference preference,processing::ComputeReport* report){return processing::wavelets(image,{.5,.3,.2,.1,.1,.1},.001,{1,1,1},1,{},preference,report);},info.available&&!cpuOnly);
    compare("wavelets / 6 scales + RGB8 preview",[&](processing::ComputePreference preference,processing::ComputeReport* report){std::vector<std::uint8_t> pixels(image.rgb.size());const processing::PreviewTarget target{pixels,std::size_t(image.width)*3};return processing::wavelets(image,{.5,.3,.2,.1,.1,.1},.001,{1,1,1},1,{},preference,report,&target);},info.available&&!cpuOnly);
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
