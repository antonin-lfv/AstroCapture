#include <astro/domain/ImageLimits.hpp>
#include <astro/processing/Mosaic.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>
#include <limits>
#include <set>
#include <stdexcept>
namespace astro::processing {
namespace {
void checkpoint(const CheckCancel& check) { if(check) check(); }
struct Features { cv::Mat gray, descriptors; std::vector<cv::KeyPoint> points; double scale{}; };
struct Pair { int i{},j{}; std::vector<cv::Point2f> a,b; double logRatio{}; };
double median(std::vector<double> v) {
    if(v.empty()) return 0;
    auto middle=v.begin()+v.size()/2; std::nth_element(v.begin(),middle,v.end()); return *middle;
}
void validate(const MosaicTile& tile, const MosaicSource& source) {
    const auto& image=tile.image;
    if(image.width!=source.width || image.height!=source.height || image.rgb.size()!=std::size_t(image.width)*image.height*3 ||
       (!tile.mask.empty() && tile.mask.size()!=std::size_t(image.width)*image.height))
        throw std::runtime_error("An input image changed or could not be read: "+source.name);
}
Features features(const MosaicTile& tile) {
    Features f;
    const auto& image=tile.image;
    f.scale=std::min(1.0,1600.0/std::max(image.width,image.height));
    cv::Mat input(image.height,image.width,CV_32FC3,const_cast<float*>(image.rgb.data())), reduced, gray;
    cv::resize(input,reduced,{},f.scale,f.scale,cv::INTER_AREA);
    cv::cvtColor(reduced,gray,cv::COLOR_RGB2GRAY);
    gray.convertTo(f.gray,CV_8U,255);
    cv::Mat mask;
    if(!tile.mask.empty()) cv::resize(cv::Mat(image.height,image.width,CV_8U,const_cast<std::uint8_t*>(tile.mask.data())),mask,f.gray.size(),0,0,cv::INTER_NEAREST);
    // Improve weak solar/lunar surface contrast for matching only. Photometry
    // and exported pixels continue to use the original, unmodified image.
    cv::Mat detail;
    cv::Mat smooth; cv::GaussianBlur(f.gray,smooth,{},1.2);
    cv::createCLAHE(2.0, {8,8})->apply(smooth, detail);
    cv::SIFT::create(3500,3,.018)->detectAndCompute(detail,mask,f.points,f.descriptors);
    return f;
}
std::optional<Pair> match(int i,int j,const Features& a,const Features& b) {
    if(a.descriptors.rows<12 || b.descriptors.rows<12) return {};
    cv::BFMatcher matcher(cv::NORM_L2);
    std::vector<std::vector<cv::DMatch>> forward,reverse;
    matcher.knnMatch(a.descriptors,b.descriptors,forward,2);
    matcher.knnMatch(b.descriptors,a.descriptors,reverse,2);
    std::vector<cv::Point2f> pa,pb;
    std::vector<double> ratio;
    for(const auto& m:forward) {
        if(m.size()<2 || m[0].distance>=.72*m[1].distance) continue;
        const auto& r=reverse[m[0].trainIdx];
        if(r.size()<2 || r[0].trainIdx!=m[0].queryIdx || r[0].distance>=.72*r[1].distance) continue;
        const auto p=a.points[m[0].queryIdx].pt,q=b.points[m[0].trainIdx].pt;
        pa.push_back(p/float(a.scale)); pb.push_back(q/float(b.scale));
        auto brightness=[](const cv::Mat& gray,cv::Point2f p) {
            const int x=std::clamp(int(p.x),3,gray.cols-4), y=std::clamp(int(p.y),3,gray.rows-4);
            return cv::mean(gray(cv::Rect(x-3,y-3,7,7)))[0];
        };
        const double av=brightness(a.gray,p),bv=brightness(b.gray,q);
        ratio.push_back(av>8 && bv>8 && av<245 && bv<245 ? std::log(av/bv) : std::numeric_limits<double>::quiet_NaN());
    }
    if(pa.size()<12) return {};
    cv::Mat inliers;
    const double threshold=3.0/std::min(a.scale,b.scale);
    const auto transform=cv::estimateAffinePartial2D(pb,pa,inliers,cv::RANSAC,threshold,3000,.995,10);
    if(transform.empty()) return {};
    const double scale=std::hypot(transform.at<double>(0,0),transform.at<double>(1,0));
    if(scale<.7 || scale>1.4) return {};
    Pair out; out.i=i; out.j=j;
    std::vector<double> ratios;
    for(std::size_t k=0;k<pa.size();++k) if(inliers.at<unsigned char>(int(k))) {
        out.a.push_back(pa[k]); out.b.push_back(pb[k]); if(std::isfinite(ratio[k])) ratios.push_back(ratio[k]);
    }
    if(out.a.size()<10 || out.a.size()<pa.size()*.4) return {};
    // Reject matches concentrated on one tiny feature or a repeated caption.
    const auto bounds=cv::boundingRect(out.a);
    if(bounds.width*a.scale<35 || bounds.height*a.scale<35) return {};
    out.logRatio=std::clamp(median(ratios),-std::log(2.0),std::log(2.0));
    return out;
}
cv::Point2d point(const cv::Matx23d& t,cv::Point2d p) { return {t(0,0)*p.x+t(0,1)*p.y+t(0,2),t(1,0)*p.x+t(1,1)*p.y+t(1,2)}; }
}
MosaicResult buildMosaic(const std::vector<MosaicSource>& inputSources,const LoadMosaicTile& loadSource,bool balanceExposure,const CheckCancel& check,const MosaicProgress& progress,bool skipUnmatched,bool softenEdges) {
    auto sources=inputSources;
    int n=int(sources.size());
    std::vector<int> indexMap(n); std::iota(indexMap.begin(),indexMap.end(),0);
    const auto load=[&](int i){return loadSource(indexMap[i]);};
    std::vector<int> excluded;
    if(n<2 || n>100) throw std::runtime_error("Choose between 2 and 100 overlapping images.");
    for(const auto& s:sources) if(s.width<64 || s.height<64 || std::uint64_t(s.width)*s.height>24000000)
        throw std::runtime_error("Each image must be at least 64 pixels per side and no larger than 24 megapixels: "+s.name);
    auto report=[&](int value,const char* stage) { checkpoint(check); if(progress) progress(value,stage); };
    std::vector<Features> all;
    for(int i=0;i<n;++i) {
        report(i*25/n,"features"); const auto tile=load(i); validate(tile,sources[i]);
        all.push_back(features(tile));
        if(!skipUnmatched && all.back().points.size()<12) throw std::runtime_error("Not enough surface detail in: "+sources[i].name);
    }
    std::set<std::pair<int,int>> candidates;
    if(n<=24) {
        for(int i=0;i<n;++i) for(int j=i+1;j<n;++j) candidates.emplace(i,j);
    } else {
        // A shared descriptor index proposes neighbors without comparing every
        // possible image pair. Geometric verification still decides each link.
        cv::FlannBasedMatcher index;
        std::vector<cv::Mat> descriptors; std::vector<int> descriptorIndices;
        for(int i=0;i<n;++i) if(!all[i].descriptors.empty()) {descriptors.push_back(all[i].descriptors);descriptorIndices.push_back(i);}
        if(!descriptors.empty()) { index.add(descriptors); index.train(); }
        for(int i=0;i<n;++i) {
            checkpoint(check); if(all[i].descriptors.empty()) continue;
            std::vector<std::vector<cv::DMatch>> neighbors;
            index.knnMatch(all[i].descriptors,neighbors,6);
            std::vector<int> votes(n,0),order(n); std::iota(order.begin(),order.end(),0);
            for(const auto& row:neighbors) for(const auto& m:row) if(descriptorIndices[m.imgIdx]!=i && m.distance<220) ++votes[descriptorIndices[m.imgIdx]];
            std::stable_sort(order.begin(),order.end(),[&](int a,int b){return votes[a]>votes[b];});
            for(int k=0;k<std::min(10,n);++k) if(order[k]!=i && votes[order[k]]>0) candidates.emplace(std::min(i,order[k]),std::max(i,order[k]));
            for(int j=i+1;j<std::min(n,i+4);++j) candidates.emplace(i,j);
        }
    }
    std::vector<Pair> pairs;
    int done=0;
    for(const auto& [i,j]:candidates) {
        report(25+(++done)*30/int(candidates.size()),"matching");
        if(auto pair=match(i,j,all[i],all[j])) pairs.push_back(std::move(*pair));
    }
    std::vector<int> group(n); std::iota(group.begin(),group.end(),0);
    auto leader=[&](int i){while(group[i]!=i)i=group[i];return i;};
    for(const auto& p:pairs) group[leader(p.j)]=leader(p.i);
    std::vector<int> sizes(n),strength(n);
    for(int i=0;i<n;++i) ++sizes[leader(i)];
    for(const auto& p:pairs) strength[leader(p.i)]+=int(p.a.size());
    int best=leader(0);
    for(int i=0;i<n;++i) if(sizes[i]>sizes[best] || (sizes[i]==sizes[best] && strength[i]>strength[best])) best=i;
    if(skipUnmatched && sizes[best]<2)
        throw std::runtime_error("No pair of images has a reliable overlap. No mosaic was created; original files are unchanged.");
    if(!skipUnmatched) {
        for(int i=0;i<n;++i) if(leader(i)!=leader(0)) throw std::runtime_error("No reliable overlap for: "+sources[i].name+". Add an intermediate view or remove this image.");
    } else if(sizes[best]<n) {
        std::vector<int> remap(n,-1), kept;
        std::vector<MosaicSource> selectedSources; std::vector<Features> selectedFeatures;
        for(int i=0;i<n;++i) {
            if(leader(i)==best) {remap[i]=int(kept.size());kept.push_back(indexMap[i]);selectedSources.push_back(sources[i]);selectedFeatures.push_back(std::move(all[i]));}
            else excluded.push_back(indexMap[i]);
        }
        std::vector<Pair> selectedPairs;
        for(auto& p:pairs) if(remap[p.i]>=0 && remap[p.j]>=0) {p.i=remap[p.i];p.j=remap[p.j];selectedPairs.push_back(std::move(p));}
        indexMap=std::move(kept); sources=std::move(selectedSources); all=std::move(selectedFeatures); pairs=std::move(selectedPairs); n=int(sources.size());
    }
    report(57,"alignment");
    // Solve all overlap constraints together, with tile zero fixing the gauge.
    // Coordinates in thousands of pixels avoid poor numerical conditioning.
    const int variables=4*(n-1);
    cv::Mat normal=cv::Mat::zeros(variables,variables,CV_64F),rhs=cv::Mat::zeros(variables,1,CV_64F);
    int matches=0;
    for(const auto& p:pairs) {
        checkpoint(check); matches+=int(p.a.size());
        const int count=std::min(100,int(p.a.size()));
        for(int k=0;k<count;++k) {
            const int at=k*int(p.a.size())/count;
            std::vector<std::pair<int,double>> rx,ry;
            double bx=0,by=0;
            auto add=[&](int tile,cv::Point2f raw,double sign) {
                const double x=raw.x/1000.,y=raw.y/1000.;
                if(tile==0) { bx-=sign*x; by-=sign*y; return; }
                const int c=(tile-1)*4;
                rx.insert(rx.end(),{{c,sign*x},{c+1,-sign*y},{c+2,sign}});
                ry.insert(ry.end(),{{c,sign*y},{c+1,sign*x},{c+3,sign}});
            };
            add(p.i,p.a[at],1); add(p.j,p.b[at],-1);
            auto equation=[&](const auto& terms,double target) {
                for(auto [i,v]:terms) { rhs.at<double>(i)+=v*target; for(auto [j,w]:terms) normal.at<double>(i,j)+=v*w; }
            };
            equation(rx,bx); equation(ry,by);
        }
    }
    cv::Mat solution;
    if(!cv::solve(normal,rhs,solution,cv::DECOMP_SVD)) throw std::runtime_error("The image positions could not be resolved.");
    std::vector<cv::Matx23d> transforms(n); transforms[0]={1,0,0,0,1,0};
    for(int i=1;i<n;++i) {
        const int k=(i-1)*4; const double a=solution.at<double>(k),b=solution.at<double>(k+1);
        if(!std::isfinite(a) || std::hypot(a,b)<.65 || std::hypot(a,b)>1.5) throw std::runtime_error("Inconsistent image scales. Use images from the same setup.");
        transforms[i]={a,-b,solution.at<double>(k+2)*1000,b,a,solution.at<double>(k+3)*1000};
    }
    std::vector<double> errors;
    for(const auto& p:pairs) {
        std::vector<double> local;
        for(std::size_t k=0;k<p.a.size();++k) local.push_back(cv::norm(point(transforms[p.i],p.a[k])-point(transforms[p.j],p.b[k])));
        const double residual=median(local);
        if(residual>5.0/std::min(all[p.i].scale,all[p.j].scale)) throw std::runtime_error("Overlaps disagree. Remove a changed, blurred or mismatched view and try again.");
        errors.push_back(residual);
    }
    std::vector<double> gains(n,1);
    if(balanceExposure) {
        cv::Mat E=cv::Mat::zeros(int(pairs.size()),n-1,CV_64F),v=cv::Mat::zeros(int(pairs.size()),1,CV_64F),g;
        for(int k=0;k<int(pairs.size());++k) {
            const auto& p=pairs[k]; if(p.i) E.at<double>(k,p.i-1)=-1; if(p.j) E.at<double>(k,p.j-1)=1;
            v.at<double>(k)=p.logRatio;
        }
        cv::solve(E,v,g,cv::DECOMP_SVD);
        for(int i=1;i<n;++i) gains[i]=std::clamp(std::exp(g.at<double>(i-1)),.25,4.);
    }
    double left=0,top=0,right=sources[0].width,bottom=sources[0].height;
    for(int i=0;i<n;++i) for(auto corner:std::array<cv::Point2d,4>{{{0,0},{double(sources[i].width),0},{0,double(sources[i].height)},{double(sources[i].width),double(sources[i].height)}}}) {
        const auto p=point(transforms[i],corner);
        left=std::min(left,p.x); top=std::min(top,p.y); right=std::max(right,p.x); bottom=std::max(bottom,p.y);
    }
    left=std::floor(left); top=std::floor(top);
    if(!std::isfinite(right) || !std::isfinite(bottom) || right-left>32700 || bottom-top>32700 || (right-left)*(bottom-top)>astro::domain::maximumImagePixels)
        throw std::runtime_error("The mosaic exceeds the 128-megapixel limit. Use smaller images or fewer tiles.");
    const int width=int(std::ceil(right-left)),height=int(std::ceil(bottom-top));
    if(!domain::supportedImageSize(width,height))throw std::runtime_error("The mosaic exceeds the 128-megapixel limit. Use smaller images or fewer tiles.");
    domain::requireImageMemory(std::uint64_t(width)*height*44+512ULL*1024*1024);
    cv::Mat sum=cv::Mat::zeros(height,width,CV_32FC3),weight=cv::Mat::zeros(height,width,CV_32F);
    for(int i=0;i<n;++i) {
        report(60+i*35/n,"blending");
        const auto tile=load(i); validate(tile,sources[i]); const auto& image=tile.image;
        auto t=transforms[i]; t(0,2)-=left; t(1,2)-=top;
        cv::Mat inverse; cv::invertAffineTransform(cv::Mat(t),inverse);
        double tileLeft=width,tileTop=height,tileRight=0,tileBottom=0;
        for(auto corner:std::array<cv::Point2d,4>{{{0,0},{double(image.width),0},{0,double(image.height)},{double(image.width),double(image.height)}}}) {
            const auto p=point(t,corner); tileLeft=std::min(tileLeft,p.x); tileTop=std::min(tileTop,p.y); tileRight=std::max(tileRight,p.x); tileBottom=std::max(tileBottom,p.y);
        }
        const int xBegin=std::max(0,int(std::floor(tileLeft))), xEnd=std::min(width,int(std::ceil(tileRight)));
        const int yBegin=std::max(0,int(std::floor(tileTop))), yEnd=std::min(height,int(std::ceil(tileBottom)));
        const double feather=std::clamp(std::min(image.width,image.height)*.15,16.,200.);
        // Inverse sampling in strips avoids allocating one mosaic-sized image
        // per tile. Alpha and edge distance provide normalized overlap weights.
        for(int y=yBegin;y<yEnd;++y) {
            checkpoint(check); auto* sums=sum.ptr<cv::Vec3f>(y); auto* weights=weight.ptr<float>(y);
            for(int x=xBegin;x<xEnd;++x) {
                const double sx=inverse.at<double>(0,0)*x+inverse.at<double>(0,1)*y+inverse.at<double>(0,2);
                const double sy=inverse.at<double>(1,0)*x+inverse.at<double>(1,1)*y+inverse.at<double>(1,2);
                const int trim=softenEdges?2:0;
                if(sx<trim || sy<trim || sx>image.width-1-trim || sy>image.height-1-trim) continue;
                const int x0=int(sx),y0=int(sy),x1=std::min(x0+1,image.width-1),y1=std::min(y0+1,image.height-1);
                const double fx=sx-x0,fy=sy-y0;
                const std::array<std::size_t,4> at{std::size_t(y0)*image.width+x0,std::size_t(y0)*image.width+x1,std::size_t(y1)*image.width+x0,std::size_t(y1)*image.width+x1};
                const std::array<double,4> blend{(1-fx)*(1-fy),fx*(1-fy),(1-fx)*fy,fx*fy};
                float alpha=0; cv::Vec3f color{};
                for(int k=0;k<4;++k) {
                    const float w=float(blend[k]*(tile.mask.empty()?1:tile.mask[at[k]]/255.)); alpha+=w;
                    for(int c=0;c<3;++c) color[c]+=image.rgb[at[k]*3+c]*w;
                }
                if(alpha<.001f) continue;
                const float edge=float(std::min({sx+1,sy+1,image.width-sx,image.height-sy,feather})/feather);
                sums[x]+=color*float(edge*gains[i]); weights[x]+=edge*alpha;
            }
        }
    }
    report(96,"finishing");
    MosaicResult out; out.image={width,height,std::vector<float>(std::size_t(width)*height*3)};
    for(int y=0;y<height;++y) {
        checkpoint(check); const auto* sums=sum.ptr<cv::Vec3f>(y); const auto* weights=weight.ptr<float>(y);
        for(int x=0;x<width;++x) for(int c=0;c<3;++c)
            out.image.rgb[(std::size_t(y)*width+x)*3+c]=weights[x]>0 ? sums[x][c]/weights[x] : 0;
    }
    if(softenEdges) {
        // Only the boundary of measured coverage is tapered, never the limb of
        // the Moon/Sun. Padding also accounts for coverage touching the canvas.
        cv::Mat support, padded, distance;
        cv::compare(weight,0,support,cv::CMP_GT);
        cv::copyMakeBorder(support,padded,1,1,1,1,cv::BORDER_CONSTANT,cv::Scalar(0));
        cv::distanceTransform(padded,distance,cv::DIST_L2,3);
        out.edgeMask.resize(std::size_t(width)*height);
        const float fade=std::clamp(std::min(width,height)*.025f,16.f,80.f);
        for(int y=0;y<height;++y) {checkpoint(check);for(int x=0;x<width;++x) {
            const auto at=std::size_t(y)*width+x;
            const float t=std::clamp((distance.at<float>(y+1,x+1)-1.f)/fade,0.f,1.f);
            const float w=t*t*t*(t*(t*6-15)+10); // Smooth value and derivatives at both ends.
            out.edgeMask[at]=w;
            for(int c=0;c<3;++c) out.image.rgb[at*3+c]*=w;
        }}
    }
    out.usedIndices=indexMap; out.excludedIndices=excluded;
    out.pairs=int(pairs.size()); out.matches=matches; out.residual=median(errors);
    report(100,"done"); return out;
}
}
