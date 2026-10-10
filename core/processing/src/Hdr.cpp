#include <astro/processing/DeepSky.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace astro::processing {
HdrResult mergeHdr(const std::vector<HdrInput>& inputs,const ImageLoader& load,double transition,const CheckCancel& check) {
    auto checkpoint=[&]{if(check)check();};
    if(inputs.size()<2||inputs.size()>8)throw std::runtime_error("HDR requires between two and eight calibrated linear images.");
    if(!std::isfinite(transition)||transition<.01||transition>.5)throw std::runtime_error("HDR transition must be between 0.01 and 0.5.");
    for(const auto& in:inputs)if(!std::isfinite(in.exposure)||in.exposure<=0||in.exposure>1e7||!std::isfinite(in.saturation)||in.saturation<=0||in.saturation>1e12)
        throw std::runtime_error("HDR exposure and saturation must be finite positive values.");
    std::vector<int> order(inputs.size());std::iota(order.begin(),order.end(),0);
    std::stable_sort(order.begin(),order.end(),[&](int a,int b){return inputs[a].exposure>inputs[b].exposure;});
    if(inputs[order.front()].exposure/inputs[order.back()].exposure<1.1)throw std::runtime_error("HDR needs different individual exposure durations (ratio at least 1.1).");
    checkpoint();auto first=load(order.front());
    if(first.bayer!=domain::BayerPattern::None)throw std::runtime_error("HDR requires debayered RGB or monochrome images, not raw CFA.");
    const auto reference=linearRgb(first,check);first.samples.clear();first.samples.shrink_to_fit();const auto pixels=std::size_t(reference.width)*reference.height;
    HdrResult result;result.image=reference;
    result.recoveredMap={reference.width,reference.height,std::vector<float>(pixels*3)};
    result.clippedMap=result.recoveredMap;
    result.alignments.resize(inputs.size());
    std::vector<float> sum(pixels*3),weights(pixels);
    std::vector<unsigned char> referenceClipped(pixels),usable(pixels);
    const double referenceExposure=inputs[order.front()].exposure;
    for(int index:order) {
        checkpoint();std::optional<AstroImage> loaded;if(index!=order.front())loaded=load(index);const auto& raw=loaded?*loaded:first;
        if(raw.width!=first.width||raw.height!=first.height||raw.channels!=first.channels||raw.bayer!=domain::BayerPattern::None)
            throw std::runtime_error("HDR sources must have identical dimensions and channel layouts.");
        if((!first.camera.empty()&&!raw.camera.empty()&&first.camera!=raw.camera)||(!first.filter.empty()&&!raw.filter.empty()&&first.filter!=raw.filter)||
           (first.hasGain&&raw.hasGain&&std::abs(first.gain-raw.gain)>1e-6)||(first.hasOffset&&raw.hasOffset&&std::abs(first.offset-raw.offset)>1e-6)||
           (first.binX&&raw.binX&&first.binX!=raw.binX)||(first.binY&&raw.binY&&first.binY!=raw.binY)||std::abs(first.scale-raw.scale)>1e-6*std::max(first.scale,raw.scale))
            throw std::runtime_error("HDR camera, filter, gain, offset, binning or sample units are incompatible.");
        std::optional<FloatImage> rgb;if(index!=order.front())rgb=linearRgb(raw,check);const auto& source=rgb?*rgb:reference;
        for(float v:source.rgb)if(!std::isfinite(v))throw std::runtime_error("HDR sources must contain only finite samples.");
        StarAlignment alignment;if(index!=order.front())alignment=alignStars(reference,source,check);
        if(alignment.residual>1)throw std::runtime_error("HDR alignment residual exceeds one pixel. Review source quality and overlap.");
        result.alignments[index]=alignment;
        const auto& m=alignment.matrix;const double det=m[0]*m[4]-m[1]*m[3];
        if(!std::isfinite(det)||std::abs(det)<1e-12)throw std::runtime_error("Invalid HDR alignment.");
        const double exposure=inputs[index].exposure,saturation=inputs[index].saturation;
        for(int y=0;y<reference.height;++y) {
            checkpoint();
            for(int x=0;x<reference.width;++x) {
                const auto p=std::size_t(y)*reference.width+x;
                double sx=(m[4]*(x-m[2])-m[1]*(y-m[5]))/det,sy=(-m[3]*(x-m[2])+m[0]*(y-m[5]))/det;
                // Numerical roundoff at an integer border is not missing sky.
                if(sx< -1e-5||sy< -1e-5||sx>source.width-1+1e-5||sy>source.height-1+1e-5)continue;
                sx=std::clamp(sx,0.,double(source.width-1));sy=std::clamp(sy,0.,double(source.height-1));
                const int x0=int(std::floor(sx)),y0=int(std::floor(sy)),x1=std::min(x0+1,source.width-1),y1=std::min(y0+1,source.height-1);
                const double fx=sx-x0,fy=sy-y0;
                const std::array<std::size_t,4> at{std::size_t(y0)*source.width+x0,std::size_t(y0)*source.width+x1,std::size_t(y1)*source.width+x0,std::size_t(y1)*source.width+x1};
                const std::array<double,4> fractions{(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
                std::array<double,3> value{};double peak=-1e300;
                // Saturation is checked in the original samples, before
                // interpolation can lower a clipped peak. One RGB weight is
                // used for every channel, avoiding channel-specific seams.
                for(int k=0;k<4;++k)if(fractions[k]>1e-10)for(int c=0;c<3;++c){const double v=source.rgb[at[k]*3+c];value[c]+=v*fractions[k];peak=std::max(peak,v);}
                if(index==order.front())referenceClipped[p]=peak>=saturation;
                const double factor=referenceExposure/exposure;
                for(int c=0;c<3;++c){const double v=value[c]*factor;if(!std::isfinite(v)||std::abs(v)>1e30)throw std::runtime_error("HDR radiance exceeds the supported floating-point range.");result.image.rgb[p*3+c]=float(v);}
                if(peak>=saturation)continue;
                usable[p]=1;
                double t=std::clamp((saturation-peak)/(saturation*transition),0.,1.);
                const double weight=(exposure/referenceExposure)*t*t*(3-2*t);
                if(weight<=0)continue;
                weights[p]+=float(weight);for(int c=0;c<3;++c)sum[p*3+c]+=float(value[c]*factor*weight);
            }
        }
    }
    for(std::size_t p=0;p<pixels;++p) {
        if(p%65536==0)checkpoint();
        if(weights[p]>0)for(int c=0;c<3;++c)result.image.rgb[p*3+c]=sum[p*3+c]/weights[p];
        const bool recovered=referenceClipped[p]&&usable[p],clipped=!usable[p];
        result.recoveredPixels+=recovered;result.clippedPixels+=clipped;
        for(int c=0;c<3;++c){result.recoveredMap.rgb[p*3+c]=recovered?1:0;result.clippedMap.rgb[p*3+c]=clipped?1:0;}
    }
    checkpoint();return result;
}
}
