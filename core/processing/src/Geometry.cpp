#include <astro/processing/Geometry.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <array>
#include <stdexcept>
namespace astro::processing {
FloatImage transformGeometry(const FloatImage& image,const Geometry& g,const CheckCancel& check) {
    auto checkpoint=[&]{if(check)check();};checkpoint();
    auto validSize=[](int w,int h){return w>0&&h>0&&std::uint64_t(w)*h<=32000000;};
    if(!validSize(image.width,image.height)||image.rgb.size()!=std::size_t(image.width)*image.height*3||!std::isfinite(g.angle)||std::abs(g.angle)>180||!std::isfinite(g.scale)||g.scale<.1||g.scale>4||g.interpolation<0||g.interpolation>4)
        throw std::runtime_error("Invalid geometry parameters.");
    if(g.crop&&(g.x<0||g.y<0||g.width<1||g.height<1||std::int64_t(g.x)+g.width>image.width||std::int64_t(g.y)+g.height>image.height))
        throw std::runtime_error("The crop rectangle exceeds the image boundaries.");
    FloatImage cropped;
    if(g.crop){cropped.width=g.width;cropped.height=g.height;cropped.rgb.resize(std::size_t(g.width)*g.height*3);for(int y=0;y<g.height;++y){checkpoint();std::copy_n(image.rgb.data()+(std::size_t(y+g.y)*image.width+g.x)*3,std::size_t(g.width)*3,cropped.rgb.data()+std::size_t(y)*g.width*3);}}
    const auto& source=g.crop?cropped:image;
    if(g.angle==0&&g.scale==1)return g.crop?std::move(cropped):image;
    const int modes[]{cv::INTER_NEAREST,cv::INTER_LINEAR,cv::INTER_CUBIC,cv::INTER_AREA,cv::INTER_LANCZOS4};
    cv::Mat input(source.height,source.width,CV_32FC3,const_cast<float*>(source.rgb.data())),rotated,resized;
    if(g.angle!=0){
        const double rad=g.angle*std::numbers::pi/180,c=std::abs(std::cos(rad)),s=std::abs(std::sin(rad));
        const int w=g.keepSize?source.width:int(std::ceil(source.width*c+source.height*s-1e-10)),h=g.keepSize?source.height:int(std::ceil(source.width*s+source.height*c-1e-10));
        if(!validSize(w,h))throw std::runtime_error("The transformed image exceeds 32 million pixels.");
        auto matrix=cv::getRotationMatrix2D(cv::Point2f((source.width-1)*.5f,(source.height-1)*.5f),-g.angle,1);
        matrix.at<double>(0,2)+=(w-source.width)*.5;matrix.at<double>(1,2)+=(h-source.height)*.5;
        checkpoint();cv::warpAffine(input,rotated,matrix,cv::Size(w,h),modes[g.interpolation],cv::BORDER_CONSTANT,cv::Scalar(0,0,0));input=rotated;checkpoint();
    }
    if(g.scale!=1){const int w=std::max(1,int(std::lround(input.cols*g.scale))),h=std::max(1,int(std::lround(input.rows*g.scale)));if(!validSize(w,h))throw std::runtime_error("The transformed image exceeds 32 million pixels.");cv::resize(input,resized,cv::Size(w,h),0,0,modes[g.interpolation]);input=resized;checkpoint();}
    FloatImage out{input.cols,input.rows,std::vector<float>(std::size_t(input.cols)*input.rows*3)};
    std::array<float,3> lo,hi;lo.fill(std::numeric_limits<float>::max());hi.fill(std::numeric_limits<float>::lowest());
    if(g.clamp)for(std::size_t i=0;i<source.rgb.size();++i){if(i%65536==0)checkpoint();lo[i%3]=std::min(lo[i%3],source.rgb[i]);hi[i%3]=std::max(hi[i%3],source.rgb[i]);}
    // Include the zero padding introduced by rotation in the permitted range.
    if(g.angle!=0)for(int c=0;c<3;++c){lo[c]=std::min(0.f,lo[c]);hi[c]=std::max(0.f,hi[c]);}
    for(int y=0;y<input.rows;++y){checkpoint();const auto* row=input.ptr<float>(y);auto* target=out.rgb.data()+std::size_t(y)*input.cols*3;for(int i=0;i<input.cols*3;++i){if(!std::isfinite(row[i]))throw std::runtime_error("Invalid transformed image samples.");target[i]=g.clamp?std::clamp(row[i],lo[i%3],hi[i%3]):row[i];}}
    return out;
}
}
