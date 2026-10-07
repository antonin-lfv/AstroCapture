#include <astro/processing/Surface.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <numeric>
#include <limits>
namespace astro::processing {
namespace {
cv::Mat gray(const FloatImage& image) {
    if (image.width < 16 || image.height < 16 || image.rgb.size() != std::size_t(image.width)*image.height*3)
        throw std::runtime_error("Invalid surface image.");
    cv::Mat out;
    cv::cvtColor(cv::Mat(image.height, image.width, CV_32FC3, const_cast<float*>(image.rgb.data())), out, cv::COLOR_RGB2GRAY);
    cv::GaussianBlur(out, out, {3,3}, .6);
    return out;
}
void checkpoint(const CheckCancel& check) { if (check) check(); }
}
struct SurfaceReference::Impl {
    cv::Mat reference;
    int patch{}, nx{}, ny{};
    std::vector<cv::Rect> patches;
    std::vector<bool> useful;
};
SurfaceReference::SurfaceReference(const FloatImage& image, int patchSize) {
    if (patchSize != 64 && patchSize != 96 && patchSize != 160)
        throw std::runtime_error("Unsupported alignment area size.");
    auto data = std::make_shared<Impl>();
    data->reference = gray(image);
    data->patch = std::min({patchSize, image.width, image.height});
    // Half-overlapping areas; cap the grid for predictable work on 5 MP frames.
    data->nx = std::clamp(image.width / (patchSize / 2), 1, 48);
    data->ny = std::clamp(image.height / (patchSize / 2), 1, 36);
    for (int y=0; y<data->ny; ++y) for (int x=0; x<data->nx; ++x) {
        const int cx = int((x+.5)*image.width/data->nx), cy = int((y+.5)*image.height/data->ny);
        const cv::Rect box(std::clamp(cx-data->patch/2,0,image.width-data->patch),
                           std::clamp(cy-data->patch/2,0,image.height-data->patch), data->patch, data->patch);
        cv::Scalar mean, sigma; cv::meanStdDev(data->reference(box), mean, sigma);
        data->patches.push_back(box);
        data->useful.push_back(mean[0] > .015 && mean[0] < .98 && sigma[0] > .006);
    }
    impl_ = std::move(data);
}
int SurfaceReference::areaCount() const { return int(impl_->patches.size()); }
std::vector<float> SurfaceReference::qualities(const FloatImage& image, Translation global, const CheckCancel& check) const {
    const auto& d=*impl_; const auto source=gray(image);
    if(source.size()!=d.reference.size() || !std::isfinite(global.x) || !std::isfinite(global.y)) throw std::runtime_error("Invalid surface frame.");
    cv::Mat gx,gy,energy,sum,light;
    cv::Sobel(source,gx,CV_32F,1,0,3); cv::Sobel(source,gy,CV_32F,0,1,3);
    energy=gx.mul(gx)+gy.mul(gy); cv::integral(energy,sum,CV_64F); cv::integral(source,light,CV_64F);
    std::vector<float> scores(d.patches.size());
    for(std::size_t i=0;i<scores.size();++i) {
        checkpoint(check); auto box=d.patches[i];
        box.x+=int(std::lround(global.x)); box.y+=int(std::lround(global.y));
        if(box.x<0 || box.y<0 || box.br().x>image.width || box.br().y>image.height) { scores[i]=-std::numeric_limits<float>::infinity(); continue; }
        if(!d.useful[i]) continue; // Flat reference areas use the global ranking as the tie-breaker.
        box.x+=2;box.y+=2;box.width-=4;box.height-=4;
        auto mean=[&](const cv::Mat& integral){return (integral.at<double>(box.br())-integral.at<double>(box.y,box.br().x)-integral.at<double>(box.br().y,box.x)+integral.at<double>(box.tl()))/box.area();};
        const double brightness=mean(light);
        scores[i]=float(mean(sum)/(brightness*brightness+.0025));
    }
    return scores;
}
std::vector<float> SurfaceReference::selectionWeights(std::span<const float> values) const {
    if(values.size()!=impl_->patches.size()) throw std::runtime_error("Invalid surface selection.");
    cv::Mat grid(impl_->ny,impl_->nx,CV_32F,const_cast<float*>(values.data())), full;
    cv::resize(grid,full,impl_->reference.size(),0,0,cv::INTER_LINEAR);
    return {full.ptr<float>(),full.ptr<float>()+full.total()};
}
SurfaceWarp SurfaceReference::align(const FloatImage& image, Translation global, const CheckCancel& check) const {
    const auto& d = *impl_;
    if (!std::isfinite(global.x) || !std::isfinite(global.y)) throw std::runtime_error("Invalid surface translation.");
    const auto source = gray(image);
    if (source.size() != d.reference.size()) throw std::runtime_error("Surface frames must have equal dimensions.");
    cv::Mat dx = cv::Mat::zeros(d.ny,d.nx,CV_32F), dy=dx.clone(), weights=dx.clone();
    int tracked=0, candidates=0;
    constexpr int radius=8;
    for (int y=0;y<d.ny;++y) for(int x=0;x<d.nx;++x) {
        checkpoint(check);
        const int i=y*d.nx+x;
        if (!d.useful[i]) continue;
        ++candidates;
        const auto box=d.patches[i];
        const int sx=box.x+int(std::lround(global.x))-radius, sy=box.y+int(std::lround(global.y))-radius;
        const cv::Rect search(sx,sy,box.width+2*radius,box.height+2*radius);
        if (sx<0 || sy<0 || search.br().x>image.width || search.br().y>image.height) continue;
        cv::Mat scores; cv::matchTemplate(source(search),d.reference(box),scores,cv::TM_CCOEFF_NORMED);
        double peak; cv::Point p; cv::minMaxLoc(scores,nullptr,&peak,nullptr,&p);
        if (peak<.65 || p.x==0 || p.y==0 || p.x==scores.cols-1 || p.y==scores.rows-1) continue;
        // A second distant peak indicates a repeating/ambiguous texture.
        cv::Mat alternatives=scores.clone();
        alternatives(cv::Rect(std::max(0,p.x-2),std::max(0,p.y-2),std::min(scores.cols,p.x+3)-std::max(0,p.x-2),std::min(scores.rows,p.y+3)-std::max(0,p.y-2))).setTo(-1);
        double second; cv::minMaxLoc(alternatives,nullptr,&second);
        if (peak-second < .002) continue;
        auto refine=[&](bool horizontal) {
            const double a=scores.at<float>(p.y-(horizontal?0:1),p.x-(horizontal?1:0));
            const double b=scores.at<float>(p.y+(horizontal?0:1),p.x+(horizontal?1:0));
            const double denom=a-2*peak+b;
            return denom < -1e-8 ? std::clamp(.5*(a-b)/denom,-.5,.5) : 0.;
        };
        const float confidence=float(peak*peak);
        dx.at<float>(y,x)=float(sx+p.x+refine(true)-box.x-global.x)*confidence;
        dy.at<float>(y,x)=float(sy+p.y+refine(false)-box.y-global.y)*confidence;
        weights.at<float>(y,x)=confidence; ++tracked;
    }
    if (tracked<3) { dx.setTo(0); dy.setTo(0); weights.setTo(0); tracked=0; }
    cv::GaussianBlur(dx,dx,{},.65); cv::GaussianBlur(dy,dy,{},.65); cv::GaussianBlur(weights,weights,{},.65);
    for(int y=0;y<d.ny;++y) for(int x=0;x<d.nx;++x) {
        const float w=weights.at<float>(y,x);
        dx.at<float>(y,x)=w>.05f ? dx.at<float>(y,x)/w : 0;
        dy.at<float>(y,x)=w>.05f ? dy.at<float>(y,x)/w : 0;
    }
    cv::Mat fullX,fullY; cv::resize(dx,fullX,source.size(),0,0,cv::INTER_LINEAR); cv::resize(dy,fullY,source.size(),0,0,cv::INTER_LINEAR);
    double minX,maxX,minY,maxY; cv::minMaxLoc(fullX,&minX,&maxX); cv::minMaxLoc(fullY,&minY,&maxY);
    SurfaceWarp out;
    out.left=std::max(0,int(std::ceil(-global.x-minX)));
    out.top=std::max(0,int(std::ceil(-global.y-minY)));
    out.right=std::min(image.width,int(std::floor(image.width-1-global.x-maxX))+1);
    out.bottom=std::min(image.height,int(std::floor(image.height-1-global.y-maxY))+1);
    out.tracked=tracked; out.candidates=candidates;
    if (out.right<=out.left || out.bottom<=out.top) throw std::runtime_error("Surface frames have no common area.");
    for(int y=0;y<image.height;++y) { checkpoint(check); for(int x=0;x<image.width;++x) {
        fullX.at<float>(y,x)+=float(x+global.x); fullY.at<float>(y,x)+=float(y+global.y);
    }}
    out.image={image.width,image.height,std::vector<float>(image.rgb.size())};
    cv::Mat input(image.height,image.width,CV_32FC3,const_cast<float*>(image.rgb.data()));
    cv::Mat output(image.height,image.width,CV_32FC3,out.image.rgb.data());
    // Bounded chunks keep cancellation responsive, including the resampling.
    for(int y=0;y<image.height;y+=128) {
        checkpoint(check); const auto rows=cv::Range(y,std::min(y+128,image.height));
        cv::Mat dst=output.rowRange(rows); cv::remap(input,dst,fullX.rowRange(rows),fullY.rowRange(rows),cv::INTER_LINEAR,cv::BORDER_REFLECT_101);
    }
    return out;
}
LocalStackResult stackLocalSelection(const std::vector<int>& indices,int keepCount,const std::function<FloatImage(int)>& load,int patchSize,const CheckCancel& check,const std::function<void(int,int)>& progress) {
    if(indices.empty() || keepCount<1) throw std::runtime_error("No frames selected.");
    const auto ref=load(indices.front()); const SurfaceReference local(ref,patchSize);
    const auto globalReference=makeAlignmentReference(ref); const int zones=local.areaCount();
    // Bound score storage independently of full-resolution video size.
    if(std::uint64_t(zones)*indices.size()>32000000) throw std::runtime_error("Too many frames for local selection. Use larger alignment areas or a shorter video.");
    struct Entry {int index; Translation offset; std::vector<float> scores;};
    std::vector<Entry> frames; int rejected=0;
    for(std::size_t i=0;i<indices.size();++i) {
        checkpoint(check); const auto image=i==0?ref:load(indices[i]);
        auto offset=i==0?Translation{0,0,1}:alignToReference(globalReference,image,check);
        if(i>0 && offset.correlation<.5) {
            // A sharp reference patch may be blurred in this frame while other
            // regions are excellent. Accept broad registration only when several
            // independent local areas also confirm it.
            auto broad=alignTranslation(globalReference.coarse,luminance(image),check);
            broad.x*=globalReference.sampleStep; broad.y*=globalReference.sampleStep;
            if(broad.correlation>=.5) {
                const auto validation=local.align(image,broad,check);
                if(validation.tracked>=std::max(3,validation.candidates/10)) offset=broad;
            }
        }
        if(offset.correlation<.5) ++rejected;
        else frames.push_back({indices[i],offset,local.qualities(image,offset,check)});
        if(progress)progress(int(i+1),int(indices.size()*2));
    }
    if(indices.size()>1 && frames.size()<2) throw std::runtime_error("Not enough reliably aligned frames for local selection.");
    keepCount=std::min(keepCount,int(frames.size()));
    std::vector<std::vector<float>> chosen(frames.size(),std::vector<float>(zones));
    std::vector<int> order(frames.size());
    for(int zone=0;zone<zones;++zone) {
        checkpoint(check); std::iota(order.begin(),order.end(),0);
        std::stable_sort(order.begin(),order.end(),[&](int a,int b){return frames[a].scores[zone]>frames[b].scores[zone];});
        int valid=0; while(valid<keepCount && std::isfinite(frames[order[valid]].scores[zone])) ++valid;
        // The reference always covers its own zones, so every area has support.
        if(!valid) throw std::runtime_error("An alignment area has no usable frames.");
        for(int k=0;k<valid;++k)chosen[order[k]][zone]=1.f/valid;
    }
    for(auto& frame:frames) { frame.scores.clear(); frame.scores.shrink_to_fit(); }
    FloatImage sum{ref.width,ref.height,std::vector<float>(ref.rgb.size())};
    std::vector<float> weights(std::size_t(ref.width)*ref.height);
    int left=0,top=0,right=ref.width,bottom=ref.height,used=0,corrected=0;
    for(std::size_t i=0;i<frames.size();++i) {
        checkpoint(check);
        if(std::any_of(chosen[i].begin(),chosen[i].end(),[](float w){return w>0;})) {
            const auto image=load(frames[i].index);
            const auto warped=local.align(image,frames[i].offset,check);
            left=std::max(left,warped.left);top=std::max(top,warped.top);right=std::min(right,warped.right);bottom=std::min(bottom,warped.bottom);
            const auto mask=local.selectionWeights(chosen[i]);
            for(int y=0;y<ref.height;++y) {checkpoint(check); for(int x=0;x<ref.width;++x) {
                const auto at=std::size_t(y)*ref.width+x;const float w=mask[at];weights[at]+=w;
                for(int c=0;c<3;++c)sum.rgb[at*3+c]+=warped.image.rgb[at*3+c]*w;
            }}
            ++used; if(warped.tracked>0)++corrected;
        }
        if(progress)progress(int(indices.size()+i+1),int(indices.size()+frames.size()));
    }
    if(right<=left || bottom<=top)throw std::runtime_error("Surface frames have no common area.");
    LocalStackResult out; out.image={right-left,bottom-top,std::vector<float>(std::size_t(right-left)*(bottom-top)*3)};
    for(int y=0;y<out.image.height;++y) {checkpoint(check);for(int x=0;x<out.image.width;++x) {
        const auto at=std::size_t(y+top)*ref.width+x+left;
        if(weights[at]<=0) throw std::runtime_error("The local stack contains an uncovered area.");
        for(int c=0;c<3;++c)out.image.rgb[(std::size_t(y)*out.image.width+x)*3+c]=sum.rgb[at*3+c]/weights[at];
    }}
    out.left=left;out.top=top;out.usedFrames=used;out.rejectedFrames=rejected;out.correctedFrames=corrected;out.areas=zones;out.framesPerArea=keepCount;return out;
}

}
