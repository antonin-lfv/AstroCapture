#include <metal_stdlib>
using namespace metal;
struct FinishParams {
    uint width,height,stretch,useAsinh;
    float black,white,midtones,strength,saturation,denoise,red,green,blue;
};
struct WaveletParams {
    uint width,height,step,vertical;
    float gain,denoise,red,green,blue,gamma;
};
inline float3 readRGB(device const float* source,uint index){return float3(source[index*3],source[index*3+1],source[index*3+2]);}
inline void writeRGB(device float* destination,uint index,float3 rgb){destination[index*3]=rgb.r;destination[index*3+1]=rgb.g;destination[index*3+2]=rgb.b;}
kernel void finishPixels(device const float* source [[buffer(0)]],device float* destination [[buffer(1)]],constant FinishParams& p [[buffer(2)]],uint2 at [[thread_position_in_grid]]) {
    if(at.x>=p.width||at.y>=p.height)return;
    const uint index=at.y*p.width+at.x;
    float3 rgb=readRGB(source,index)*float3(p.red,p.green,p.blue);
    if(p.stretch) {
        rgb=clamp((rgb-p.black)/(p.white-p.black),0.f,1.f);
        if(p.useAsinh) {
            const float maximum=max(max(rgb.r,rgb.g),rgb.b);
            const float factor=maximum>1e-8f?asinh(p.strength*maximum)/(asinh(p.strength)*maximum):1.f;
            rgb*=factor;
        }else rgb=((p.midtones-1.f)*rgb)/((2.f*p.midtones-1.f)*rgb-p.midtones);
    }
    if(p.saturation!=1.f) {
        const float l=.2126f*rgb.r+.7152f*rgb.g+.0722f*rgb.b;
        const float protect=clamp((l-.03f)/.12f,0.f,1.f);
        const float amount=1.f+(p.saturation-1.f)*protect*protect*(3.f-2.f*protect);
        rgb=l+(rgb-l)*amount;
        if(p.stretch)rgb=clamp(rgb,0.f,1.f);
    }
    writeRGB(destination,index,rgb);
}
inline int reflect101(int value,int length) {
    if(length<=1)return 0;
    while(value<0||value>=length)value=value<0?-value:2*length-value-2;
    return value;
}
kernel void smoothHorizontal(device const float* source [[buffer(0)]],device float* destination [[buffer(1)]],constant FinishParams& p [[buffer(2)]],constant float* weights [[buffer(3)]],uint2 at [[thread_position_in_grid]]) {
    if(at.x>=p.width||at.y>=p.height)return;
    float3 sum=0.f;
    for(int k=-3;k<=3;++k)sum+=readRGB(source,at.y*p.width+reflect101(int(at.x)+k,p.width))*weights[k+3];
    writeRGB(destination,at.y*p.width+at.x,sum);
}
kernel void smoothVerticalBlend(device const float* source [[buffer(0)]],device float* destination [[buffer(1)]],constant FinishParams& p [[buffer(2)]],constant float* weights [[buffer(3)]],uint2 at [[thread_position_in_grid]]) {
    if(at.x>=p.width||at.y>=p.height)return;
    float3 sum=0.f;
    for(int k=-3;k<=3;++k)sum+=readRGB(source,reflect101(int(at.y)+k,p.height)*p.width+at.x)*weights[k+3];
    const uint index=at.y*p.width+at.x;
    writeRGB(destination,index,readRGB(destination,index)*(1.f-p.denoise)+sum*p.denoise);
}
kernel void waveletBlur(device const float* source [[buffer(0)]],device float* destination [[buffer(1)]],constant WaveletParams& p [[buffer(2)]],uint2 at [[thread_position_in_grid]]) {
    if(at.x>=p.width||at.y>=p.height)return;
    constexpr float weights[5]={1.f/16.f,4.f/16.f,6.f/16.f,4.f/16.f,1.f/16.f};
    float3 sum=0.f;
    for(int k=-2;k<=2;++k) {
        int x=clamp(int(at.x)+(p.vertical?0:k*int(p.step)),0,int(p.width)-1);
        int y=clamp(int(at.y)+(p.vertical?k*int(p.step):0),0,int(p.height)-1);
        sum+=readRGB(source,uint(y)*p.width+uint(x))*weights[k+2];
    }
    writeRGB(destination,at.y*p.width+at.x,sum);
}
kernel void waveletAdd(device const float* current [[buffer(0)]],device const float* blurred [[buffer(1)]],constant WaveletParams& p [[buffer(2)]],device float* output [[buffer(3)]],uint2 at [[thread_position_in_grid]]) {
    if(at.x>=p.width||at.y>=p.height)return;
    const uint index=at.y*p.width+at.x;
    const float3 detail=readRGB(current,index)-readRGB(blurred,index);
    writeRGB(output,index,readRGB(output,index)+p.gain*copysign(max(0.f,abs(detail)-p.denoise),detail));
}
kernel void waveletColor(device float* output [[buffer(0)]],constant WaveletParams& p [[buffer(2)]],uint2 at [[thread_position_in_grid]]) {
    if(at.x>=p.width||at.y>=p.height)return;
    const uint index=at.y*p.width+at.x;
    writeRGB(output,index,pow(max(0.f,readRGB(output,index)*float3(p.red,p.green,p.blue)),1.f/p.gamma));
}

struct PreviewParams {uint width,height;};
kernel void previewPixels(device const float* source [[buffer(0)]],device uchar* destination [[buffer(1)]],constant PreviewParams& p [[buffer(2)]],uint2 at [[thread_position_in_grid]]) {
    if(at.x>=p.width||at.y>=p.height)return;
    const uint index=(at.y*p.width+at.x)*3;
    for(uint c=0;c<3;++c)destination[index+c]=uchar(round(clamp(source[index+c],0.f,1.f)*255.f));
}
