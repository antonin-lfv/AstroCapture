#include "MetalProcessing.hpp"
#include "MetalShaders.hpp"
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <thread>

namespace astro::processing {
namespace {
void checkpoint(const CheckCancel& check){if(check)check();}
bool disabled(){const char* value=std::getenv("ASTRO_DISABLE_METAL");return value&&std::strcmp(value,"1")==0;}
struct Context {
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    std::map<std::string,id<MTLComputePipelineState>> pipelines;
    std::string reason;
    Context(){@autoreleasepool {
        device=MTLCreateSystemDefaultDevice();
        if(!device){reason="No Metal device is available.";return;}
        if(![device supportsFamily:MTLGPUFamilyApple7]){reason="This Metal backend requires Apple Silicon.";device=nil;return;}
        queue=[device newCommandQueue];if(!queue){reason="Cannot create the Metal command queue.";return;}
        MTLCompileOptions* options=[MTLCompileOptions new];
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 150000
        if(@available(macOS 15.0,*)){options.mathMode=MTLMathModeSafe;options.mathFloatingPointFunctions=MTLMathFloatingPointFunctionsPrecise;}
        else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            options.fastMathEnabled=NO;
#pragma clang diagnostic pop
        }
#else
        options.fastMathEnabled=NO;
#endif
        NSError* error=nil;id<MTLLibrary> library=[device newLibraryWithSource:[NSString stringWithUTF8String:detail::metalShaderSource] options:options error:&error];
        if(!library){reason=error?error.localizedDescription.UTF8String:"Cannot compile Metal processing kernels.";return;}
        for(const char* name:{"previewPixels","finishPixels","smoothHorizontal","smoothVerticalBlend","waveletBlur","waveletAdd","waveletColor"}) {
            id<MTLFunction> function=[library newFunctionWithName:[NSString stringWithUTF8String:name]];
            id<MTLComputePipelineState> pipeline=function?[device newComputePipelineStateWithFunction:function error:&error]:nil;
            if(!pipeline){reason=error?error.localizedDescription.UTF8String:"Cannot create a Metal processing pipeline.";pipelines.clear();return;}
            pipelines.emplace(name,pipeline);
        }
    }}
};
Context& context(){static Context instance;return instance;}
struct FinishParams {std::uint32_t width,height,stretch,useAsinh;float black,white,midtones,strength,saturation,denoise,red,green,blue;};
struct WaveletParams {std::uint32_t width,height,step,vertical;float gain,denoise,red,green,blue,gamma;};
static_assert(sizeof(FinishParams)==52&&sizeof(WaveletParams)==40);
bool ready(const FloatImage& image,int buffers,int maximumRows,const CheckCancel& check,std::string& reason) {
    checkpoint(check);
    if(disabled()){reason="Metal is disabled by ASTRO_DISABLE_METAL.";return false;}
    auto& ctx=context();if(!ctx.reason.empty()){reason=ctx.reason;return false;}
    const auto bytes=std::size_t(image.width)*std::min(image.height,maximumRows)*3*sizeof(float);
    if(bytes>ctx.device.maxBufferLength || bytes*buffers>ctx.device.recommendedMaxWorkingSetSize/2){reason="The image exceeds the Metal memory budget.";return false;}
    for(std::size_t i=0;i<image.rgb.size();++i){if(i%65536==0)checkpoint(check);if(!std::isfinite(image.rgb[i])){reason="Non-finite samples require CPU processing.";return false;}}
    return true;
}
id<MTLBuffer> allocate(std::size_t bytes){return [context().device newBufferWithLength:bytes options:MTLResourceStorageModeShared];}
void copySamples(void* to,const void* from,std::size_t bytes,const CheckCancel& check){
    constexpr std::size_t chunk=4*1024*1024;
    for(std::size_t offset=0;offset<bytes;offset+=chunk){checkpoint(check);std::memcpy(static_cast<char*>(to)+offset,static_cast<const char*>(from)+offset,std::min(chunk,bytes-offset));}
}
// Resource lifetime extends until completion even when the cancellation callback
// throws. Never publish partially computed pixels or free buffers still in use.
bool complete(id<MTLCommandBuffer> command,const CheckCancel& check,std::string& reason,ComputeReport* report) {
    [command commit];if(report)++report->submittedCommands;std::exception_ptr cancelled;
    while(command.status!=MTLCommandBufferStatusCompleted&&command.status!=MTLCommandBufferStatusError) {
        if(!cancelled)try{checkpoint(check);}catch(...){cancelled=std::current_exception();}
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if(report)++report->finishedCommands;
    if(cancelled)std::rethrow_exception(cancelled);checkpoint(check);
    if(command.status!=MTLCommandBufferStatusCompleted){reason=command.error?command.error.localizedDescription.UTF8String:"Metal command execution failed.";return false;}
    return true;
}
void dispatch(id<MTLComputeCommandEncoder> encoder,const char* name,int width,int height) {
    auto pipeline=context().pipelines.at(name);[encoder setComputePipelineState:pipeline];
    const auto group=MTLSizeMake(pipeline.threadExecutionWidth,std::max(NSUInteger(1),std::min(NSUInteger(8),pipeline.maxTotalThreadsPerThreadgroup/pipeline.threadExecutionWidth)),1);
    [encoder dispatchThreads:MTLSizeMake(width,height,1) threadsPerThreadgroup:group];
}
}
MetalAvailability metalAvailability(){@autoreleasepool {
    if(disabled())return {false,{},"Metal is disabled by ASTRO_DISABLE_METAL."};
    auto& ctx=context();return {ctx.reason.empty(),ctx.device?ctx.device.name.UTF8String:"",ctx.reason};
}}
namespace detail {
bool metalPreview(const FloatImage& image,std::span<std::uint8_t> destination,std::size_t stride,const CheckCancel& check,std::string& reason,ComputeReport* report){@autoreleasepool {
    constexpr int rows=512;if(!ready(image,2,rows,check,reason))return false;
    const auto rowBytes=std::size_t(image.width)*3;
    const auto capacity=rowBytes*std::min(image.height,rows);
    id<MTLBuffer> input=allocate(capacity*sizeof(float)),output=allocate(capacity);
    if(!input||!output){reason="Cannot allocate shared Metal preview buffers.";return false;}
    struct Params{std::uint32_t width,height;};Params p{std::uint32_t(image.width),0};
    for(int y=0;y<image.height;y+=rows){
        checkpoint(check);p.height=std::min(rows,image.height-y);copySamples(input.contents,image.rgb.data()+std::size_t(y)*image.width*3,p.height*rowBytes*sizeof(float),check);
        id<MTLCommandBuffer> command=[context().queue commandBuffer];id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
        if(!command||!encoder){reason="Cannot encode Metal preview.";return false;}
        [encoder setBuffer:input offset:0 atIndex:0];[encoder setBuffer:output offset:0 atIndex:1];[encoder setBytes:&p length:sizeof(p) atIndex:2];dispatch(encoder,"previewPixels",image.width,p.height);[encoder endEncoding];
        if(!complete(command,check,reason,report))return false;
        for(unsigned row=0;row<p.height;++row){checkpoint(check);std::memcpy(destination.data()+(y+row)*stride,static_cast<const char*>(output.contents)+row*rowBytes,rowBytes);}
    }
    return true;
}}
bool metalFinish(const FloatImage& image,const Finish& settings,FloatImage& result,const CheckCancel& check,std::string& reason,const PreviewTarget* preview,ComputeReport* report){@autoreleasepool {
    constexpr int rows=512;const bool smooth=settings.denoise>0;const int halo=smooth?3:0;
    if(!ready(image,(smooth?3:2)+(preview?1:0),rows+2*halo,check,reason))return false;
    const auto rowBytes=std::size_t(image.width)*3*sizeof(float);
    const auto capacity=rowBytes*std::min(image.height,rows+2*halo);
    id<MTLBuffer> input=allocate(capacity),output=allocate(capacity),scratch=smooth?allocate(capacity):nil;
    id<MTLBuffer> previewOutput=preview?allocate(capacity/sizeof(float)):nil;
    if(!input||!output||(smooth&&!scratch)||(preview&&!previewOutput)){reason="Cannot allocate shared Metal buffers.";return false;}
    FloatImage out{image.width,image.height,std::vector<float>(image.rgb.size())};
    FinishParams p{std::uint32_t(image.width),0,std::uint32_t(settings.stretch),std::uint32_t(settings.asinh),settings.black,settings.white,settings.midtones,settings.asinhStrength,settings.saturation,std::clamp(settings.denoise,0.f,1.f),settings.color[0],settings.color[1],settings.color[2]};
    // Identical 7-tap sigma=.7 Gaussian and REFLECT_101 borders as the CPU.
    float weights[7];double sum=0;for(int k=-3;k<=3;++k)sum+=std::exp(-double(k*k)/(2*.7*.7));
    for(int k=-3;k<=3;++k)weights[k+3]=float(std::exp(-double(k*k)/(2*.7*.7))/sum);
    for(int y=0;y<image.height;y+=rows) {
        checkpoint(check);const int count=std::min(rows,image.height-y),top=std::max(0,y-halo),bottom=std::min(image.height,y+count+halo);p.height=bottom-top;
        copySamples(input.contents,image.rgb.data()+std::size_t(top)*image.width*3,p.height*rowBytes,check);
        id<MTLCommandBuffer> command=[context().queue commandBuffer];id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
        if(!command||!encoder){reason="Cannot encode a Metal command.";return false;}
        [encoder setBuffer:input offset:0 atIndex:0];[encoder setBuffer:output offset:0 atIndex:1];[encoder setBytes:&p length:sizeof(p) atIndex:2];dispatch(encoder,"finishPixels",image.width,p.height);[encoder endEncoding];
        if(smooth){
            encoder=[command computeCommandEncoder];if(!encoder){reason="Cannot encode Metal smoothing.";return false;}
            [encoder setBuffer:output offset:0 atIndex:0];[encoder setBuffer:scratch offset:0 atIndex:1];[encoder setBytes:&p length:sizeof(p) atIndex:2];[encoder setBytes:weights length:sizeof(weights) atIndex:3];dispatch(encoder,"smoothHorizontal",image.width,p.height);[encoder endEncoding];
            encoder=[command computeCommandEncoder];if(!encoder){reason="Cannot encode Metal smoothing.";return false;}
            [encoder setBuffer:scratch offset:0 atIndex:0];[encoder setBuffer:output offset:0 atIndex:1];[encoder setBytes:&p length:sizeof(p) atIndex:2];[encoder setBytes:weights length:sizeof(weights) atIndex:3];dispatch(encoder,"smoothVerticalBlend",image.width,p.height);[encoder endEncoding];
        }
        if(preview){
            struct PreviewParams{std::uint32_t width,height;};PreviewParams pp{p.width,p.height};
            id<MTLComputeCommandEncoder> pe=[command computeCommandEncoder];if(!pe){reason="Cannot encode fused Metal preview.";return false;}
            [pe setBuffer:output offset:0 atIndex:0];[pe setBuffer:previewOutput offset:0 atIndex:1];[pe setBytes:&pp length:sizeof(pp) atIndex:2];dispatch(pe,"previewPixels",image.width,p.height);[pe endEncoding];
        }
        if(!complete(command,check,reason,report))return false;
        copySamples(out.rgb.data()+std::size_t(y)*image.width*3,static_cast<const char*>(output.contents)+(y-top)*rowBytes,count*rowBytes,check);
        if(preview)for(int row=0;row<count;++row){checkpoint(check);std::memcpy(preview->pixels.data()+std::size_t(y+row)*preview->bytesPerLine,static_cast<const char*>(previewOutput.contents)+std::size_t(y-top+row)*image.width*3,std::size_t(image.width)*3);}
    }
    result=std::move(out);return true;
}}
bool metalWavelets(const FloatImage& image,const std::array<float,6>& gains,float denoise,const std::array<float,3>& color,float gamma,FloatImage& result,const CheckCancel& check,std::string& reason,const PreviewTarget* preview,ComputeReport* report){@autoreleasepool {
    int last=-1;for(int i=0;i<6;++i)if(gains[i]!=0)last=i;
    // The support of successive B3 scales is the sum of their radii, not just
    // the largest radius. Overlap guarantees identical values at strip seams.
    constexpr int rows=512;const int halo=last<0?0:2*((1<<(last+1))-1);
    if(!ready(image,(last>=0?4:1)+(preview?1:0),rows+2*halo,check,reason))return false;
    const auto rowBytes=std::size_t(image.width)*3*sizeof(float);
    const auto capacity=rowBytes*std::min(image.height,rows+2*halo);
    id<MTLBuffer> output=allocate(capacity),current=last>=0?allocate(capacity):nil,temp=last>=0?allocate(capacity):nil,blurred=last>=0?allocate(capacity):nil;
    id<MTLBuffer> previewOutput=preview?allocate(capacity/sizeof(float)):nil;
    if(!output||(last>=0&&(!current||!temp||!blurred))||(preview&&!previewOutput)){reason="Cannot allocate shared Metal wavelet buffers.";return false;}
    FloatImage out{image.width,image.height,std::vector<float>(image.rgb.size())};
    WaveletParams p{std::uint32_t(image.width),0,1,0,0,denoise,color[0],color[1],color[2],gamma};
    for(int y=0;y<image.height;y+=rows) {
        checkpoint(check);const int count=std::min(rows,image.height-y),top=std::max(0,y-halo),bottom=std::min(image.height,y+count+halo);p.height=bottom-top;
        const auto* source=image.rgb.data()+std::size_t(top)*image.width*3;
        copySamples(output.contents,source,p.height*rowBytes,check);if(last>=0)copySamples(current.contents,source,p.height*rowBytes,check);
        id<MTLCommandBuffer> command=[context().queue commandBuffer];if(!command){reason="Cannot encode Metal wavelets.";return false;}
        for(int level=0;level<=last;++level){
            checkpoint(check);p.step=1u<<level;p.gain=gains[level];
            for(int pass=0;pass<2;++pass){
                id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];if(!encoder){reason="Cannot encode Metal wavelets.";return false;}
                p.vertical=pass;[encoder setBuffer:pass==0?current:temp offset:0 atIndex:0];[encoder setBuffer:pass==0?temp:blurred offset:0 atIndex:1];[encoder setBytes:&p length:sizeof(p) atIndex:2];dispatch(encoder,"waveletBlur",image.width,p.height);[encoder endEncoding];
            }
            id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];if(!encoder){reason="Cannot encode Metal wavelets.";return false;}
            [encoder setBuffer:current offset:0 atIndex:0];[encoder setBuffer:blurred offset:0 atIndex:1];[encoder setBuffer:output offset:0 atIndex:3];[encoder setBytes:&p length:sizeof(p) atIndex:2];dispatch(encoder,"waveletAdd",image.width,p.height);[encoder endEncoding];
            std::swap(current,blurred);
        }
        id<MTLComputeCommandEncoder> encoder=[command computeCommandEncoder];
        if(!encoder){reason="Cannot encode Metal color adjustment.";return false;}
        [encoder setBuffer:output offset:0 atIndex:0];[encoder setBytes:&p length:sizeof(p) atIndex:2];dispatch(encoder,"waveletColor",image.width,p.height);[encoder endEncoding];
        if(preview){
            struct PreviewParams{std::uint32_t width,height;};PreviewParams pp{p.width,p.height};
            id<MTLComputeCommandEncoder> pe=[command computeCommandEncoder];if(!pe){reason="Cannot encode fused Metal preview.";return false;}
            [pe setBuffer:output offset:0 atIndex:0];[pe setBuffer:previewOutput offset:0 atIndex:1];[pe setBytes:&pp length:sizeof(pp) atIndex:2];dispatch(pe,"previewPixels",image.width,p.height);[pe endEncoding];
        }
        if(!complete(command,check,reason,report))return false;
        copySamples(out.rgb.data()+std::size_t(y)*image.width*3,static_cast<const char*>(output.contents)+(y-top)*rowBytes,count*rowBytes,check);
        if(preview)for(int row=0;row<count;++row){checkpoint(check);std::memcpy(preview->pixels.data()+std::size_t(y+row)*preview->bytesPerLine,static_cast<const char*>(previewOutput.contents)+std::size_t(y-top+row)*image.width*3,std::size_t(image.width)*3);}
    }
    result=std::move(out);return true;
}}
}
}
