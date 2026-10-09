#include <astro/processing/DeepSky.hpp>
#include "MetalProcessing.hpp"
#include <astro/processing/DeepSkyAdvanced.hpp>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/calib3d.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <unordered_map>

namespace astro::processing {
namespace {
void checked(const CheckCancel& check){if(check)check();}
float medianInPlace(std::vector<float>& values) {
    values.erase(std::remove_if(values.begin(),values.end(),[](float v){return !std::isfinite(v);}),values.end());
    if(values.empty())throw std::runtime_error("No finite image samples.");
    const auto middle=values.begin()+values.size()/2;
    std::nth_element(values.begin(),middle,values.end()); return *middle;
}
float median(std::vector<float> values) {return medianInPlace(values);}
void same(const AstroImage& a,const AstroImage& b) {
    if(a.width!=b.width||a.height!=b.height||a.channels!=b.channels||a.bayer!=b.bayer||a.samples.size()!=b.samples.size())
        throw std::runtime_error("Calibration frames must match the lights' dimensions, channels and Bayer pattern.");
}
float level(const FloatImage& image,int channel=0) {
    std::vector<float> sample;
    const auto pixels=std::size_t(image.width)*image.height;
    const auto step=std::max(std::size_t(1),pixels/30000);
    for(std::size_t i=0;i<pixels;i+=step)sample.push_back(image.rgb[i*3+channel]);
    return median(std::move(sample));
}
struct DiskImage {
    std::unique_ptr<FILE,decltype(&std::fclose)> file{std::tmpfile(),&std::fclose};
    explicit DiskImage(const FloatImage& image) {
        if(!file||std::fwrite(image.rgb.data(),sizeof(float),image.rgb.size(),file.get())!=image.rgb.size())
            throw std::runtime_error("Not enough temporary disk space for integration.");
    }
};
FloatImage warp(const FloatImage& source,const StarAlignment& alignment,int width,int height) {
    cv::Mat input(source.height,source.width,CV_32FC3,const_cast<float*>(source.rgb.data()));
    cv::Mat matrix(2,3,CV_64F,const_cast<double*>(alignment.matrix.data()));
    cv::Mat output;
    cv::warpAffine(input,output,matrix,{width,height},cv::INTER_LINEAR,cv::BORDER_CONSTANT,cv::Scalar::all(NAN));
    FloatImage image{width,height,std::vector<float>(std::size_t(width)*height*3)};
    std::memcpy(image.rgb.data(),output.ptr<float>(),image.rgb.size()*sizeof(float));
    return image;
}
// Largest rectangle inside common coverage: rotated borders never enter the output.
cv::Rect commonRectangle(const std::vector<unsigned char>& mask,int width,int height) {
    std::vector<int> heights(width),stack;
    cv::Rect best; long long area=0;
    for(int y=0;y<height;++y) {
        for(int x=0;x<width;++x)heights[x]=mask[std::size_t(y)*width+x]?heights[x]+1:0;
        stack.clear();
        for(int x=0;x<=width;++x) {
            const int h=x==width?0:heights[x];
            while(!stack.empty()&&heights[stack.back()]>h) {
                const int top=stack.back(); stack.pop_back();
                const int left=stack.empty()?0:stack.back()+1;
                const long long a=1LL*heights[top]*(x-left);
                if(a>area){area=a;best={left,y-heights[top]+1,x-left,heights[top]};}
            }
            stack.push_back(x);
        }
    }
    if(area<64)throw std::runtime_error("Images do not have enough common coverage.");
    return best;
}
}
AstroImage masterFrame(int count,const ImageLoader& load,bool normalize,const CheckCancel& check) {
    if(count<1)throw std::runtime_error("No calibration frames selected.");
    auto result=load(0); std::vector<double> sum(result.samples.size());
    std::vector<unsigned> valid(sum.size()); double exposure=0;
    for(int n=0;n<count;++n) {
        checked(check); auto image=n?load(n):result; same(result,image);
        if(result.exposure>0&&image.exposure>0&&std::abs(image.exposure-result.exposure)>0.01*result.exposure)
            throw std::runtime_error("A master must contain frames with the same exposure duration.");
        if(result.hasGain&&image.hasGain&&std::abs(result.gain-image.gain)>.01)
            throw std::runtime_error("A master must contain frames with the same gain.");
        if(result.hasTemperature&&image.hasTemperature&&std::abs(result.temperature-image.temperature)>2)
            throw std::runtime_error("Calibration frames differ in temperature by more than two degrees.");
        exposure+=image.exposure;
        float scale=1;
        if(normalize){scale=median(image.samples);if(!(scale>0))throw std::runtime_error("Flat frames must have positive illumination.");}
        for(std::size_t i=0;i<sum.size();++i){if(i%65536==0)checked(check);if(std::isfinite(image.samples[i])){sum[i]+=image.samples[i]/scale;++valid[i];}}
    }
    for(std::size_t i=0;i<sum.size();++i)result.samples[i]=valid[i]?float(sum[i]/valid[i]):NAN;
    result.exposure=exposure/count; return result;
}
AstroImage detectBadPixels(const AstroImage& dark,double sigma) {
    AstroImage map=dark;std::vector<float> sampled;
    const auto step=std::max(std::size_t(1),dark.samples.size()/100000);
    for(std::size_t i=0;i<dark.samples.size();i+=step)sampled.push_back(dark.samples[i]);
    const float center=median(sampled);
    std::vector<float> deviations;deviations.reserve(sampled.size());for(float v:sampled)deviations.push_back(std::abs(v-center));
    const float threshold=float(sigma)*std::max(1.f/65535.f,1.4826f*median(std::move(deviations)));
    for(int y=0;y<dark.height;++y)for(int x=0;x<dark.width;++x)for(int c=0;c<dark.channels;++c) {
        const auto at=(std::size_t(y)*dark.width+x)*dark.channels+c;
        if(std::isfinite(dark.samples[at]) && dark.samples[at]<=center+threshold){map.samples[at]=0;continue;}
        if(!std::isfinite(dark.samples[at])){map.samples[at]=1;continue;}
        std::vector<float> neighbors;
        for(int dy=-2;dy<=2;++dy)for(int dx=-2;dx<=2;++dx){const int nx=x+dx,ny=y+dy;if((dx||dy)&&nx>=0&&ny>=0&&nx<dark.width&&ny<dark.height)neighbors.push_back(dark.samples[(std::size_t(ny)*dark.width+nx)*dark.channels+c]);}
        map.samples[at]=!std::isfinite(dark.samples[at]) || (!neighbors.empty() && dark.samples[at]>center+threshold && dark.samples[at]>median(std::move(neighbors))+threshold)?1.f:0.f;
    }
    return map;
}
AstroImage calibrate(AstroImage light,const Calibration& masters,const CheckCancel& check) {
    if(masters.dark)same(light,*masters.dark);
    if(masters.flat)same(light,*masters.flat);
    if(masters.bias)same(light,*masters.bias);
    if(masters.dark&&light.exposure>0&&masters.dark->exposure>0&&std::abs(light.exposure-masters.dark->exposure)>std::max(.01,light.exposure*.01))
        throw std::runtime_error("Darks must have the same exposure as lights. Dark scaling is not applied.");
    for(const auto* frame:{masters.dark?&*masters.dark:nullptr,masters.bias?&*masters.bias:nullptr}) {
        if(!frame)continue;
        if(light.hasGain&&frame->hasGain&&std::abs(light.gain-frame->gain)>.01)
            throw std::runtime_error("Calibration gain does not match the light frame.");
        if(frame== (masters.dark?&*masters.dark:nullptr)&&light.hasTemperature&&frame->hasTemperature&&std::abs(light.temperature-frame->temperature)>2)
            throw std::runtime_error("Dark temperature differs from lights by more than 2 degrees.");
    }
    // Raw master dark includes bias: never subtract bias twice.
    for(std::size_t i=0;i<light.samples.size();++i) {
        if(i%65536==0)checked(check);
        if(masters.dark)light.samples[i]-=masters.dark->samples[i];
        else if(masters.bias)light.samples[i]-=masters.bias->samples[i];
        if(masters.flat) {
            const float flat=masters.flat->samples[i];
            light.samples[i]=std::isfinite(flat)&&flat>1e-6f?light.samples[i]/flat:NAN;
        }
    }
    if(masters.badPixels) {
        const auto& map=*masters.badPixels;
        if(map.width!=light.width||map.height!=light.height||(map.channels!=1&&map.channels!=light.channels))throw std::runtime_error("Bad pixel map dimensions do not match lights.");
        const auto original=light.samples;const int step=light.bayer==domain::BayerPattern::None?1:2;
        for(int y=0;y<light.height;++y) {
            checked(check);
            for(int x=0;x<light.width;++x)for(int c=0;c<light.channels;++c) {
                const auto at=(std::size_t(y)*light.width+x)*light.channels+c;
                if(!(map.samples[(std::size_t(y)*map.width+x)*map.channels+(map.channels==1?0:c)]>0))continue;
                std::vector<float> neighbors;
                for(int dy=-step;dy<=step;dy+=step)for(int dx=-step;dx<=step;dx+=step) {
                    const int nx=x+dx,ny=y+dy;if(!(dx||dy)||nx<0||ny<0||nx>=light.width||ny>=light.height)continue;
                    const auto p=std::size_t(ny)*light.width+nx;
                    if(map.samples[p*map.channels+(map.channels==1?0:c)]>0)continue;
                    const auto value=original[p*light.channels+c];if(std::isfinite(value))neighbors.push_back(value);
                }
                light.samples[at]=neighbors.empty()?NAN:median(std::move(neighbors));
            }
        }
    }
    return light;
}
FloatImage linearRgb(const AstroImage& image,const CheckCancel& check) {
    if(image.samples.size()!=std::size_t(image.width)*image.height*image.channels||(image.channels!=1&&image.channels!=3))
        throw std::runtime_error("Invalid astronomical image.");
    FloatImage result{image.width,image.height,std::vector<float>(std::size_t(image.width)*image.height*3)};
    if(image.channels==3){result.rgb=image.samples;return result;}
    for(int y=0;y<image.height;++y) {
        checked(check);
        for(int x=0;x<image.width;++x)for(int c=0;c<3;++c) {
            const auto at=std::size_t(y)*image.width+x;
            if(image.bayer==domain::BayerPattern::None||domain::bayerChannelAt(image.bayer,x,y)==c){result.rgb[at*3+c]=image.samples[at];continue;}
            double sum=0;int count=0;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                const int nx=x+dx,ny=y+dy;
                if(nx<0||ny<0||nx>=image.width||ny>=image.height||domain::bayerChannelAt(image.bayer,nx,ny)!=c)continue;
                const float v=image.samples[std::size_t(ny)*image.width+nx];if(std::isfinite(v)){sum+=v;++count;}
            }
            result.rgb[at*3+c]=count?float(sum/count):image.samples[at];
        }
    }
    return result;
}
std::vector<Star> detectStars(const FloatImage& image,const CheckCancel& check,int maximumStars) {
    if(maximumStars<1||maximumStars>5000)throw std::runtime_error("Invalid star detection limit.");
    checked(check);
    const double resize=std::min(1.,1600./std::max(image.width,image.height));
    cv::Mat rgb(image.height,image.width,CV_32FC3,const_cast<float*>(image.rgb.data())),gray,small;
    cv::cvtColor(rgb,gray,cv::COLOR_RGB2GRAY);
    if(resize<1)cv::resize(gray,small,{},resize,resize,cv::INTER_AREA);else small=gray.clone();
    cv::patchNaNs(small,0);
    cv::Mat smooth,background,signal;
    cv::GaussianBlur(small,smooth,{},.8);
    cv::GaussianBlur(small,background,{},12);
    signal=smooth-background;
    std::vector<float> values;
    for(int y=0;y<signal.rows;y+=3)for(int x=0;x<signal.cols;x+=3)values.push_back(signal.at<float>(y,x));
    const float base=median(values);for(auto& v:values)v=std::abs(v-base);
    const float noise=std::max(1e-7f,median(values)*1.4826f);
    cv::Mat mask=signal>base+5*noise,labels,stats,centers;
    const int count=cv::connectedComponentsWithStats(mask,labels,stats,centers);
    std::vector<Star> stars;
    for(int k=1;k<count;++k) {
        checked(check);
        const int area=stats.at<int>(k,cv::CC_STAT_AREA);
        if(area<2||area>2500)continue;
        const int left=stats.at<int>(k,cv::CC_STAT_LEFT),top=stats.at<int>(k,cv::CC_STAT_TOP);
        const int w=stats.at<int>(k,cv::CC_STAT_WIDTH),h=stats.at<int>(k,cv::CC_STAT_HEIGHT);
        if(left<3||top<3||left+w>=signal.cols-3||top+h>=signal.rows-3||std::max(w,h)>3*std::min(w,h))continue;
        double flux=0,sx=0,sy=0,sxx=0,syy=0;
        for(int y=top;y<top+h;++y)for(int x=left;x<left+w;++x)if(labels.at<int>(y,x)==k) {
            const double v=std::max(0.f,signal.at<float>(y,x)-base);flux+=v;sx+=x*v;sy+=y*v;sxx+=x*x*v;syy+=y*y*v;
        }
        if(flux<=0)continue;
        const double cx=sx/flux,cy=sy/flux;
        stars.push_back({(cx+.5)/resize-.5,(cy+.5)/resize-.5,flux/(resize*resize),
            2.355*std::sqrt(std::max(0.,(sxx/flux-cx*cx+syy/flux-cy*cy)/2))/resize});
    }
    std::sort(stars.begin(),stars.end(),[](const Star& a,const Star& b){return a.flux>b.flux;});
    if(stars.size()>std::size_t(maximumStars))stars.resize(maximumStars);return stars;
}
namespace {
struct Triangle {std::array<int,3> vertices;double a{},b{},longest{};};
std::vector<Triangle> triangles(const std::vector<Star>& stars) {
    const int n=std::min(30,int(stars.size()));std::vector<Triangle> out;
    auto distance=[&](int a,int b){return std::hypot(stars[a].x-stars[b].x,stars[a].y-stars[b].y);};
    for(int i=0;i<n;++i)for(int j=i+1;j<n;++j)for(int k=j+1;k<n;++k) {
        std::array<std::pair<double,int>,3> edges{{{distance(j,k),i},{distance(i,k),j},{distance(i,j),k}}};
        std::sort(edges.begin(),edges.end());
        if(edges[0].first<12||edges[2].first<30||edges[0].first+edges[1].first<edges[2].first*1.05)continue;
        out.push_back({{edges[0].second,edges[1].second,edges[2].second},edges[0].first/edges[2].first,edges[1].first/edges[2].first,edges[2].first});
    }return out;
}
std::vector<std::pair<int,int>> matchStars(const std::vector<Star>& reference,const std::vector<Star>& source,const std::array<double,6>& m,double limit) {
    std::vector<std::pair<int,int>> matches;std::vector<bool> used(reference.size());
    for(int s=0;s<int(source.size());++s) {
        const double x=m[0]*source[s].x+m[1]*source[s].y+m[2],y=m[3]*source[s].x+m[4]*source[s].y+m[5];
        int best=-1;double d=limit;
        for(int r=0;r<int(reference.size());++r)if(!used[r]){const double v=std::hypot(x-reference[r].x,y-reference[r].y);if(v<d){d=v;best=r;}}
        if(best>=0){matches.emplace_back(best,s);used[best]=true;}
    }return matches;
}
}
StarAlignment alignStars(const FloatImage& reference,const FloatImage& source,const CheckCancel& check) {
    auto a=detectStars(reference,check),b=detectStars(source,check);
    if(a.size()<6||b.size()<6)throw std::runtime_error("At least six identifiable stars are required for alignment.");
    const auto ta=triangles(a),tb=triangles(b);
    std::unordered_multimap<int,const Triangle*> lookup;
    for(const auto& t:ta)lookup.emplace(int(t.a*100)*101+int(t.b*100),&t);
    StarAlignment best;int bestCount=0,candidates=0;
    for(const auto& t:tb) {
        checked(check);
        for(int da=-1;da<=1;++da)for(int db=-1;db<=1;++db) {
            const auto [first,last]=lookup.equal_range((int(t.a*100)+da)*101+int(t.b*100)+db);
            for(auto it=first;it!=last;++it) {
                const auto& r=*it->second;
                const double scale=r.longest/t.longest;if(scale<.8||scale>1.2)continue;
                const auto& p=b[t.vertices[0]];const auto& q=b[t.vertices[1]];
                const auto& u=a[r.vertices[0]];const auto& v=a[r.vertices[1]];
                const double dx=q.x-p.x,dy=q.y-p.y,den=dx*dx+dy*dy;
                const double c=((v.x-u.x)*dx+(v.y-u.y)*dy)/den,s=((v.y-u.y)*dx-(v.x-u.x)*dy)/den;
                std::array<double,6> m{c,-s,u.x-c*p.x+s*p.y,s,c,u.y-s*p.x-c*p.y};
                const auto matches=matchStars(a,b,m,3.5);
                if(int(matches.size())>bestCount){bestCount=int(matches.size());best.matrix=m;}
                if(++candidates>12000||bestCount>=int(std::min(a.size(),b.size())*.8))goto found;
            }
        }
    }
found:
    if(bestCount<6)throw std::runtime_error("No reliable star alignment found. Check overlap and image quality.");
    const auto matches=matchStars(a,b,best.matrix,3.5);
    std::vector<cv::Point2f> src,dst;
    for(auto [r,s]:matches){src.emplace_back(b[s].x,b[s].y);dst.emplace_back(a[r].x,a[r].y);}
    cv::Mat inliers;
    auto matrix=cv::estimateAffinePartial2D(src,dst,inliers,cv::RANSAC,2.0,2000,.995,10);
    if(matrix.empty())throw std::runtime_error("Star alignment refinement failed.");
    std::memcpy(best.matrix.data(),matrix.ptr<double>(),6*sizeof(double));
    double error=0;best.matches=0;
    for(int i=0;i<int(src.size());++i)if(inliers.at<unsigned char>(i)) {
        const auto& p=src[i];const auto& q=dst[i];
        const double dx=best.matrix[0]*p.x+best.matrix[1]*p.y+best.matrix[2]-q.x,dy=best.matrix[3]*p.x+best.matrix[4]*p.y+best.matrix[5]-q.y;
        error+=dx*dx+dy*dy;++best.matches;
    }
    if(best.matches<6)throw std::runtime_error("Too few consistent star matches.");
    best.residual=std::sqrt(error/best.matches);
    std::vector<float> ratios;
    for(int i=0;i<int(matches.size());++i)if(inliers.at<unsigned char>(i)){const auto [r,s]=matches[i];if(b[s].flux>0)ratios.push_back(float(a[r].flux/b[s].flux));}
    if(!ratios.empty())best.fluxScale=std::clamp(double(median(std::move(ratios))),.1,10.);
    return best;
}
Integration integrate(int count,const ImageLoader& load,const Calibration& masters,bool calibrated,double sigma,const CheckCancel& check,const Progress& progress,bool normalize,const BackgroundOptions* backgroundOptions) {
    if(count<1||count>500)throw std::runtime_error("Select between 1 and 500 light frames per group.");
    if(!std::isfinite(sigma)||sigma<0||sigma>10)throw std::runtime_error("Sigma rejection must be between 0 and 10.");
    Integration result;
    auto raw=load(0);const auto pattern=raw.bayer;const int channels=raw.channels;
    result.wcs=raw.wcs;result.centerRa=raw.centerRa;result.centerDec=raw.centerDec;result.pixelScale=raw.pixelScale;result.date=raw.date;
    if(!calibrated)raw=calibrate(std::move(raw),masters,check);
    auto reference=linearRgb(raw,check);
    if(backgroundOptions)reference=extractBackground(reference,*backgroundOptions,check).image;
    std::vector<unsigned char> coverage(std::size_t(reference.width)*reference.height,1);
    std::vector<DiskImage> disk;
    std::array<float,3> background{level(reference,0),level(reference,1),level(reference,2)};
    for(int n=0;n<count;++n) {
        checked(check);if(progress)progress(n,count,"Calibration");
        try {
            auto data=n?load(n):raw;
            if(data.width!=reference.width||data.height!=reference.height||data.channels!=channels||data.bayer!=pattern)
                throw std::runtime_error("Dimensions or sensor layout differ from the reference.");
            if(!calibrated&&n)data=calibrate(std::move(data),masters,check);
            auto image=n?linearRgb(data,check):reference;
            if(n&&backgroundOptions){if(progress)progress(n,count,"Background extraction");image=extractBackground(image,*backgroundOptions,check).image;}
            if(n){
                if(progress)progress(n,count,"Star analysis and registration");
                const auto alignment=alignStars(reference,image,check);
                if(normalize)for(auto& value:image.rgb)value*=float(alignment.fluxScale);
                image=warp(image,alignment,reference.width,reference.height);
            }
            if(progress)progress(n,count,"Normalization");
            if(normalize)for(int c=0;c<3;++c){const auto offset=level(image,c)-background[c];for(std::size_t p=c;p<image.rgb.size();p+=3)if(std::isfinite(image.rgb[p]))image.rgb[p]-=offset;}
            for(std::size_t p=0;p<coverage.size();++p)coverage[p]&=std::isfinite(image.rgb[p*3])&&std::isfinite(image.rgb[p*3+1])&&std::isfinite(image.rgb[p*3+2]);
            disk.emplace_back(image);result.accepted.push_back(n);result.totalExposure+=data.exposure;
        }catch(const std::runtime_error& e){if(n==0)throw;result.rejected.push_back(n);result.reasons.emplace_back(e.what());}
    }
    if(disk.empty())throw std::runtime_error("No usable light frames.");
    const auto crop=commonRectangle(coverage,reference.width,reference.height);
    if(result.wcs){result.wcs->crpix[0]-=crop.x;result.wcs->crpix[1]-=crop.y;}
    result.image={crop.width,crop.height,std::vector<float>(std::size_t(crop.width)*crop.height*3)};
    // Disk-backed strips keep memory independent of the number of full images.
    const int rows=std::max(1,65536/(reference.width*3));
    std::vector<float> values(disk.size()),deviations(disk.size());
    for(int y=0;y<crop.height;y+=rows) {
        checked(check);if(progress)progress(y,crop.height,"Rejection and integration");
        const int h=std::min(rows,crop.height-y);
        const auto length=std::size_t(reference.width)*h*3;
        std::vector<std::vector<float>> strips(disk.size(),std::vector<float>(length));
        for(std::size_t n=0;n<disk.size();++n) {
            const auto offset=std::uint64_t(crop.y+y)*reference.width*3*sizeof(float);
#ifdef _WIN32
            const int seek=_fseeki64(disk[n].file.get(),offset,SEEK_SET);
#else
            const int seek=fseeko(disk[n].file.get(),off_t(offset),SEEK_SET);
#endif
            if(seek||std::fread(strips[n].data(),sizeof(float),length,disk[n].file.get())!=length)
                throw std::runtime_error("Cannot read temporary integration data.");
        }
        for(int yy=0;yy<h;++yy)for(int x=0;x<crop.width*3;++x) {
            const auto at=std::size_t(yy)*reference.width*3+crop.x*3+x;
            for(std::size_t n=0;n<disk.size();++n)values[n]=strips[n][at];
            if(sigma==0 || values.size()<3) {
                double sum=0;for(float v:values)sum+=v;
                result.image.rgb[(std::size_t(y+yy)*crop.width*3)+x]=float(sum/values.size());continue;
            }
            const float center=medianInPlace(values);
            for(std::size_t n=0;n<values.size();++n)deviations[n]=std::abs(values[n]-center);
            const float limit=float(sigma)*std::max(1e-7f,1.4826f*medianInPlace(deviations));
            double sum=0;int used=0;
            for(float v:values)if(std::isfinite(v)&&(values.size()<3||std::abs(v-center)<=limit)){sum+=v;++used;}else ++result.rejectedSamples;
            result.image.rgb[(std::size_t(y+yy)*crop.width*3)+x]=used?float(sum/used):center;
        }
    }
    return result;
}
BackgroundResult extractBackground(const FloatImage& input,const BackgroundOptions& options,const CheckCancel& check) {
    if(input.width<2||input.height<2||std::uint64_t(input.width)*input.height>32000000||input.rgb.size()!=std::size_t(input.width)*input.height*3)
        throw std::runtime_error("Invalid background image dimensions.");
    if(options.degree<1||options.degree>4||options.samplesPerLine<2||options.samplesPerLine>50||!std::isfinite(options.tolerance)||options.tolerance<0||options.tolerance>10||options.samples.size()>2500)
        throw std::runtime_error("Invalid background sampling settings.");
    const int terms=(options.degree+1)*(options.degree+2)/2;
    auto basis=[&](double x,double y){std::vector<double> out; x=2*x-1;y=2*y-1;for(int d=0;d<=options.degree;++d)for(int k=0;k<=d;++k)out.push_back(std::pow(x,k)*std::pow(y,d-k));return out;};
    std::vector<std::array<double,2>> points=options.samples;
    if(points.empty())for(int y=0;y<options.samplesPerLine;++y)for(int x=0;x<options.samplesPerLine;++x)points.push_back({(x+.5)/options.samplesPerLine,(y+.5)/options.samplesPerLine});
    std::array<std::vector<float>,3> values;
    for(const auto& point:points){
        checked(check);if(!std::isfinite(point[0])||!std::isfinite(point[1])||point[0]<0||point[0]>1||point[1]<0||point[1]>1)throw std::runtime_error("Background samples must lie inside the image.");
        const int cx=std::min(input.width-1,int(point[0]*input.width)),cy=std::min(input.height-1,int(point[1]*input.height));
        const int radius=std::max(1,std::min({12,input.width/(options.samplesPerLine*4),input.height/(options.samplesPerLine*4)}));
        std::array<std::vector<float>,3> patch;
        for(int y=std::max(0,cy-radius);y<=std::min(input.height-1,cy+radius);++y)for(int x=std::max(0,cx-radius);x<=std::min(input.width-1,cx+radius);++x)for(int c=0;c<3;++c){float v=input.rgb[(std::size_t(y)*input.width+x)*3+c];if(std::isfinite(v))patch[c].push_back(v);}
        for(int c=0;c<3;++c){if(patch[c].empty())throw std::runtime_error("A background sample contains no finite pixels.");values[c].push_back(median(std::move(patch[c])));}
    }
    std::array<float,3> base{},limit{};
    for(int c=0;c<3;++c){base[c]=median(values[c]);std::vector<float> deviations;for(float v:values[c])deviations.push_back(std::abs(v-base[c]));limit[c]=base[c]+float(options.tolerance)*std::max(median(std::move(deviations)),1e-8f);}
    std::vector<int> accepted;
    for(std::size_t i=0;i<points.size();++i){bool keep=!options.samples.empty();if(!keep){keep=true;for(int c=0;c<3;++c)keep&=values[c][i]<=limit[c];}if(keep)accepted.push_back(int(i));}
    if(int(accepted.size())<terms)throw std::runtime_error("Not enough background samples for this polynomial degree. Reduce degree or increase tolerance.");
    cv::Mat a(int(accepted.size()),terms,CV_64F),b(int(accepted.size()),3,CV_64F);
    for(int n=0;n<int(accepted.size());++n){int i=accepted[n];auto row=basis(points[i][0],points[i][1]);for(int k=0;k<terms;++k)a.at<double>(n,k)=row[k];for(int c=0;c<3;++c)b.at<double>(n,c)=values[c][i];}
    cv::Mat singular;cv::SVD::compute(a,singular);if(singular.rows<terms||singular.at<double>(terms-1)<singular.at<double>(0)*1e-10)throw std::runtime_error("Background samples do not constrain the model. Spread samples across the image.");
    cv::Mat coefficients;if(!cv::solve(a,b,coefficients,cv::DECOMP_SVD))throw std::runtime_error("Cannot fit background model.");
    BackgroundResult result;result.image=input;result.model={input.width,input.height,std::vector<float>(input.rgb.size())};for(int i:accepted)result.samples.push_back(points[i]);
    std::vector<std::array<double,5>> xp(input.width),yp(input.height);
    for(int x=0;x<input.width;++x){xp[x][0]=1;for(int d=1;d<=options.degree;++d)xp[x][d]=xp[x][d-1]*(2*(x+.5)/input.width-1);}
    for(int y=0;y<input.height;++y){yp[y][0]=1;for(int d=1;d<=options.degree;++d)yp[y][d]=yp[y][d-1]*(2*(y+.5)/input.height-1);}
    for(int y=0;y<input.height;++y){checked(check);for(int x=0;x<input.width;++x){std::array<double,3> v{};int term=0;for(int d=0;d<=options.degree;++d)for(int k=0;k<=d;++k,++term){const double weight=xp[x][k]*yp[y][d-k];for(int c=0;c<3;++c)v[c]+=weight*coefficients.at<double>(term,c);}for(int c=0;c<3;++c){const auto at=(std::size_t(y)*input.width+x)*3+c;result.model.rgb[at]=float(v[c]);result.image.rgb[at]-=float(v[c]-base[c]);}}}
    return result;
}
FloatImage removeGreen(const FloatImage& input,int method,float amount,bool preserve,const CheckCancel& check) {
    if(input.width<=0||input.height<=0||std::uint64_t(input.width)*input.height>32000000||input.rgb.size()!=std::size_t(input.width)*input.height*3||method<0||method>3||!std::isfinite(amount)||amount<0||amount>1)throw std::runtime_error("Invalid green noise reduction settings.");
    FloatImage out=input;
    // CIE L* preservation, in bounded RGB display space, one row at a time.
    for(int y=0;y<input.height;++y){checked(check);const auto at=std::size_t(y)*input.width*3;cv::Mat original(1,input.width,CV_32FC3,const_cast<float*>(input.rgb.data()+at)),corrected(1,input.width,CV_32FC3,out.rgb.data()+at);
        for(int x=0;x<input.width;++x){auto* p=out.rgb.data()+at+x*3;for(int c=0;c<3;++c)if(!std::isfinite(p[c])||p[c]<0||p[c]>1)throw std::runtime_error("Green reduction requires stretched RGB pixels between 0 and 1.");const float m=method==0?(p[0]+p[2])*.5f:method==3?std::min(1.f,p[0]+p[2]):std::max(p[0],p[2]);p[1]=method<2?std::min(p[1],m):p[1]*((1-amount)*(1-m)+m);}
        if(preserve){cv::Mat before,after;cv::cvtColor(original,before,cv::COLOR_RGB2Lab);cv::cvtColor(corrected,after,cv::COLOR_RGB2Lab);for(int x=0;x<input.width;++x)after.at<cv::Vec3f>(0,x)[0]=before.at<cv::Vec3f>(0,x)[0];cv::cvtColor(after,corrected,cv::COLOR_Lab2RGB);}
    }
    return out;
}
FloatImage orientImage(const FloatImage& input,int turns,bool horizontal,bool vertical,const CheckCancel& check) {
    if(input.width<=0||input.height<=0||std::uint64_t(input.width)*input.height>32000000||input.rgb.size()!=std::size_t(input.width)*input.height*3||turns<0||turns>3)throw std::runtime_error("Invalid image orientation.");
    FloatImage output;output.width=turns%2?input.height:input.width;output.height=turns%2?input.width:input.height;output.rgb.resize(input.rgb.size());
    for(int y=0;y<input.height;++y){checked(check);for(int x=0;x<input.width;++x){int tx=x,ty=y;if(turns==1){tx=input.height-1-y;ty=x;}else if(turns==2){tx=input.width-1-x;ty=input.height-1-y;}else if(turns==3){tx=y;ty=input.width-1-x;}if(horizontal)tx=output.width-1-tx;if(vertical)ty=output.height-1-ty;for(int c=0;c<3;++c)output.rgb[(std::size_t(ty)*output.width+tx)*3+c]=input.rgb[(std::size_t(y)*input.width+x)*3+c];}}
    return output;
}
FloatImage finishDeepSky(const FloatImage& input,const Finish& options,const CheckCancel& check,ComputeReport* report,const PreviewTarget* preview) {
    if(input.width<=0 || input.height<=0 || std::uint64_t(input.width)*input.height>32000000 || input.rgb.size()!=std::size_t(input.width)*input.height*3)
        throw std::runtime_error("Invalid processing image dimensions.");
    if(!std::isfinite(options.white)||!std::isfinite(options.black)||!std::isfinite(options.midtones)||!(options.white>options.black)||options.midtones<=0||options.midtones>=1)
        throw std::runtime_error("White must exceed black; midtones must be between 0 and 1.");
    if(options.asinhLuminance<0||options.asinhLuminance>2)throw std::runtime_error("Invalid asinh luminance mode.");
    if(!std::isfinite(options.asinhStrength)||options.asinhStrength<1||options.asinhStrength>1000||!std::isfinite(options.saturation)||options.saturation<0||options.saturation>3)
        throw std::runtime_error("Invalid asinh or saturation settings.");
    if(!std::isfinite(options.denoise)||std::any_of(options.color.begin(),options.color.end(),[](float v){return !std::isfinite(v);}))
        throw std::runtime_error("Invalid color or smoothing settings.");
    if(preview)validatePreviewTarget(input,*preview);
    checked(check);
    const bool metal=detail::requestMetal(input.rgb.size()/3,options.compute,report);
    auto accelerate=[&](const FloatImage& source)->std::optional<FloatImage> {
        if(!metal)return {};
        FloatImage out;std::string reason;
        if(detail::metalFinish(source,options,out,check,reason,preview,report)){if(report)report->backend=ComputeBackend::Metal;return out;}
        if(report)report->fallbackReason=std::move(reason);return {};
    };
    if(options.ghs&&report)report->fallbackReason="GHS uses the CPU reference implementation.";
    const GhsCurve ghs({options.ghsStrength,options.ghsLocal,options.ghsSymmetry,options.ghsShadows,options.ghsHighlights,options.ghsInverse,options.ghsMode});
    if(!options.background&&!options.ghs)if(auto out=accelerate(input))return std::move(*out);
    FloatImage image=input;
    if(options.background) {
        image=extractBackground(input,options.backgroundOptions,check).image;
    }
    if(options.background&&!options.ghs)if(auto out=accelerate(image))return std::move(*out);
    for(int y=0;y<image.height;++y) {
        checked(check);
        for(int x=0;x<image.width;++x) {
            auto* pixel=image.rgb.data()+(std::size_t(y)*image.width+x)*3;
            for(int c=0;c<3;++c) {
                pixel[c]*=options.color[c];
                if(options.stretch)pixel[c]=std::clamp((pixel[c]-options.black)/(options.white-options.black),0.f,1.f);
            }
            if(options.stretch) {
                if(options.ghs) {
                    const auto original=std::array<float,3>{pixel[0],pixel[1],pixel[2]};
                    if(options.ghsMode==0)for(int c=0;c<3;++c)pixel[c]=float(ghs(original[c]));
                    else {const double l=.2126*original[0]+.7152*original[1]+.0722*original[2];const double factor=l>1e-12?ghs(l)/l:1;std::array<double,3> rgb{},independent{};double blend=1;for(int c=0;c<3;++c){rgb[c]=original[c]*factor;independent[c]=ghs(original[c]);if(rgb[c]>1)blend=std::min(blend,(1-independent[c])/(rgb[c]-independent[c]));}for(int c=0;c<3;++c)pixel[c]=float(std::clamp(blend*rgb[c]+(1-blend)*independent[c],0.,1.));}
                }else if(options.asinh) {
                    const float maximum=options.asinhLuminance==1?.2126f*pixel[0]+.7152f*pixel[1]+.0722f*pixel[2]:options.asinhLuminance==2?(pixel[0]+pixel[1]+pixel[2])/3:std::max({pixel[0],pixel[1],pixel[2]});
                    const float factor=maximum>1e-8f?std::asinh(options.asinhStrength*maximum)/(std::asinh(options.asinhStrength)*maximum):1;
                    for(int c=0;c<3;++c)pixel[c]*=factor;
                }else for(int c=0;c<3;++c) {
                    const float v=pixel[c],m=options.midtones;
                    pixel[c]=((m-1)*v)/((2*m-1)*v-m);
                }
            }
            if(options.saturation!=1) {
                const float luminance=.2126f*pixel[0]+.7152f*pixel[1]+.0722f*pixel[2];
                const float protect=std::clamp((luminance-.03f)/.12f,0.f,1.f);
                const float amount=1+(options.saturation-1)*protect*protect*(3-2*protect);
                for(int c=0;c<3;++c){pixel[c]=luminance+(pixel[c]-luminance)*amount;if(options.stretch)pixel[c]=std::clamp(pixel[c],0.f,1.f);}
            }
        }
    }

    if(options.denoise>0) {
        cv::Mat pixels(image.height,image.width,CV_32FC3,image.rgb.data()),smooth;
        cv::GaussianBlur(pixels,smooth,{},.7);
        const float amount=std::clamp(options.denoise,0.f,1.f);
        for(std::size_t i=0;i<image.rgb.size();++i)image.rgb[i]=image.rgb[i]*(1-amount)+smooth.ptr<float>()[i]*amount;
    }
    if(preview)renderRgb8(image,preview->pixels,preview->bytesPerLine,check,ComputePreference::Cpu);
    return image;
}
FloatImage combineChannels(const FloatImage& red,const FloatImage& green,const FloatImage& blue,const FloatImage* luminance,const CheckCancel& check) {
    const auto ga=alignStars(red,green,check),ba=alignStars(red,blue,check);
    const auto g=warp(green,ga,red.width,red.height),b=warp(blue,ba,red.width,red.height);
    std::optional<FloatImage> l;if(luminance)l=warp(*luminance,alignStars(red,*luminance,check),red.width,red.height);
    FloatImage out=red;std::vector<unsigned char> mask(std::size_t(red.width)*red.height);
    for(std::size_t p=0;p<mask.size();++p) {
        if(p%65536==0)checked(check);
        out.rgb[p*3]=(red.rgb[p*3]+red.rgb[p*3+1]+red.rgb[p*3+2])/3;
        out.rgb[p*3+1]=(g.rgb[p*3]+g.rgb[p*3+1]+g.rgb[p*3+2])/3;
        out.rgb[p*3+2]=(b.rgb[p*3]+b.rgb[p*3+1]+b.rgb[p*3+2])/3;
        if(l){const auto brightness=(out.rgb[p*3]+out.rgb[p*3+1]+out.rgb[p*3+2])/3;const auto lv=(l->rgb[p*3]+l->rgb[p*3+1]+l->rgb[p*3+2])/3;if(brightness>1e-8)for(int c=0;c<3;++c)out.rgb[p*3+c]*=lv/brightness;}
        mask[p]=std::isfinite(out.rgb[p*3])&&std::isfinite(out.rgb[p*3+1])&&std::isfinite(out.rgb[p*3+2]);
    }
    const auto crop=commonRectangle(mask,out.width,out.height);
    FloatImage result{crop.width,crop.height,std::vector<float>(std::size_t(crop.width)*crop.height*3)};
    for(int y=0;y<crop.height;++y)std::copy_n(out.rgb.begin()+((std::size_t(y+crop.y)*out.width+crop.x)*3),crop.width*3,result.rgb.begin()+std::size_t(y)*crop.width*3);
    return result;
}
}
