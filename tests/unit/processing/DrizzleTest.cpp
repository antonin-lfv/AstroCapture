#include <astro/processing/DeepSky.hpp>
#include <astro/recording/AstroImage.hpp>
#include "Check.hpp"
#include <cmath>
#include <limits>
#include <numeric>
#include <filesystem>
#include <chrono>
using namespace astro;
namespace {
recording::AstroImage field(double dx=0,double dy=0) {
    recording::AstroImage image;image.width=64;image.height=64;image.channels=3;image.bits=32;image.exposure=120;image.samples.resize(64*64*3);
    const std::array<std::array<double,2>,10> stars{{{12,12},{30,10},{50,16},{14,30},{36,26},{50,40},{12,50},{30,50},{45,53},{26,36}}};
    for(int y=0;y<64;++y)for(int x=0;x<64;++x)for(int c=0;c<3;++c){double v=.02;for(const auto& p:stars)v+=.2*std::exp(-((x-p[0]-dx)*(x-p[0]-dx)+(y-p[1]-dy)*(y-p[1]-dy))/5.);image.samples[(y*64+x)*3+c]=float(v*(c+1));}
    return image;
}
}
int main() {
    processing::FloatImage input{8,8,std::vector<float>(8*8*3)};
    for(int p=0;p<64;++p){input.rgb[p*3]=float(p)-20;input.rgb[p*3+1]=0;input.rgb[p*3+2]=2.25;}
    const auto identity=processing::drizzleProject(input,{},8,8,{1,1});
    ASTRO_TEST_CHECK(identity.image.rgb==input.rgb);
    for(float w:identity.weights)ASTRO_TEST_CHECK(w==1);
    // A quarter-pixel shift of a full drop has analytic 0.75 / 0.25
    // overlap in each axis. Real zero and signed HDR values are retained.
    input.rgb.assign(input.rgb.size(),0);input.rgb[(3*8+3)*3]=4;
    processing::StarAlignment shifted;shifted.matrix[2]=shifted.matrix[5]=.25;
    const auto fractional=processing::drizzleProject(input,shifted,8,8,{1,1});
    for(const auto [x,y,value]:std::array<std::array<double,3>,4>{{{3,3,2.25},{4,3,.75},{3,4,.75},{4,4,.25}}}) {
        const auto p=(std::size_t(y)*8+int(x))*3;
        ASTRO_TEST_CHECK(std::abs(fractional.image.rgb[p]-value)<1e-6);
    }
    // Exact rotated polygon area: a 45-degree square stays flux-conserving
    // in sky-area units; per-pixel weights sum to one for an isolated drop.
    input.rgb.assign(input.rgb.size(),std::numeric_limits<float>::quiet_NaN());input.rgb[(3*8+3)*3]=input.rgb[(3*8+3)*3+1]=input.rgb[(3*8+3)*3+2]=2;
    processing::StarAlignment rotated;const double c=std::sqrt(.5);rotated.matrix={c,-c,3,c,c,3-6*c};
    const auto square=processing::drizzleProject(input,rotated,8,8,{2,.8});
    ASTRO_TEST_CHECK(std::abs(std::accumulate(square.weights.begin(),square.weights.end(),0.)-1)<1e-6);
    for(std::size_t p=0;p<square.weights.size();++p)if(square.weights[p]>0)for(int channel=0;channel<3;++channel)ASTRO_TEST_CHECK(std::abs(square.image.rgb[p*3+channel]-2)<1e-6);
    // Four known half-grid phases reconstruct a high-frequency analytic
    // scene. Bilinear interpolation of those same samples smooths it.
    auto scene=[](double x,double y){return 1+.3*std::sin(2.5*x)*std::cos(2.2*y);};
    std::vector<double> sum(32*32),weights(32*32),bilinear(32*32);
    for(double dy:{-.25,.25})for(double dx:{-.25,.25}) {
        processing::FloatImage sample{16,16,std::vector<float>(16*16*3)};
        for(int y=0;y<16;++y)for(int x=0;x<16;++x)for(int ch=0;ch<3;++ch)sample.rgb[(y*16+x)*3+ch]=float(scene(x+dx,y+dy));
        processing::StarAlignment m;m.matrix[2]=dx;m.matrix[5]=dy;
        const auto p=processing::drizzleProject(sample,m,16,16,{2,.5});
        for(int y=2;y<30;++y)for(int x=2;x<30;++x) {
            const int at=y*32+x;const double w=p.weights[at];if(w>0){sum[at]+=p.image.rgb[at*3]*w;weights[at]+=w;}
            const double xx=(x+.5)/2-.5-dx,yy=(y+.5)/2-.5-dy;const int ix=int(std::floor(xx)),iy=int(std::floor(yy));const double fx=xx-ix,fy=yy-iy;
            bilinear[at]+=((1-fx)*(1-fy)*scene(ix+dx,iy+dy)+fx*(1-fy)*scene(ix+1+dx,iy+dy)+(1-fx)*fy*scene(ix+dx,iy+1+dy)+fx*fy*scene(ix+1+dx,iy+1+dy))*.25;
        }
    }
    double error=0,interpolationError=0;
    for(int y=2;y<30;++y)for(int x=2;x<30;++x){const int at=y*32+x;ASTRO_TEST_CHECK(weights[at]>0);const double truth=scene((x+.5)/2-.5,(y+.5)/2-.5);error+=std::pow(sum[at]/weights[at]-truth,2);interpolationError+=std::pow(bilinear[at]-truth,2);}
    ASTRO_TEST_CHECK(error<1e-10&&interpolationError>.1);
    auto a=field();recording::CelestialWcs wcs;wcs.crval={23,30};wcs.crpix={32.5,32.5};wcs.cd={-.001,0,0,.001};wcs.sipOrder=3;wcs.sipA[12]=1e-5;wcs.sipB[2]=-2e-5;a.wcs=wcs;a.pixelScale=3.6;
    processing::DrizzleOptions options{2,.8};
    auto integrated=processing::integrate(4,[&](int n){auto f=n?field((n%2)*.5,(n/2)*.5):a;if(n==2)f.samples[(32*64+32)*3]=10;return f;},{},true,3,{}, {},false,nullptr,&options);
    ASTRO_TEST_CHECK(integrated.accepted.size()==4&&integrated.totalExposure==480&&integrated.image.width>=125&&integrated.image.height>=125&&integrated.subpixelDiversity);
    ASTRO_TEST_CHECK(integrated.rejectedSamples>0&&integrated.minimumContributors>0&&integrated.meanContributors>2);
    ASTRO_TEST_CHECK(integrated.wcs&&integrated.wcs->sipOrder==3&&std::abs(integrated.wcs->scale()-1.8)<1e-8&&*integrated.pixelScale==1.8);
    ASTRO_TEST_CHECK(std::abs(integrated.wcs->sipA[12]-wcs.sipA[12]/2)<1e-12);
    for(float v:integrated.image.rgb)ASTRO_TEST_CHECK(std::isfinite(v));
    const auto path=std::filesystem::temp_directory_path()/("astrocapture-drizzle-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".fits");
    recording::AstroImage exported=a;exported.width=integrated.image.width;exported.height=integrated.image.height;exported.samples=integrated.image.rgb;exported.wcs=integrated.wcs;exported.pixelScale=integrated.pixelScale;
    recording::writeAstroFits(path,exported);const auto restored=recording::readAstroImage(path);std::filesystem::remove(path);
    ASTRO_TEST_CHECK(restored.samples==exported.samples&&restored.wcs&&std::abs(restored.wcs->scale()-1.8)<1e-8);
    bool refused=false;try{processing::drizzleDimensions(6244,4170,{2,.8});}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    refused=false;try{processing::drizzleProject(input,{},8,8,{1,0});}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    refused=false;try{processing::integrate(1,[&](int){return a;},{},true,0,{}, {},false,nullptr,&options);}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    auto hole=a;for(int y=25;y<30;++y)for(int x=25;x<30;++x)for(int ch=0;ch<3;++ch)hole.samples[(y*64+x)*3+ch]=NAN;
    refused=false;try{processing::integrate(2,[&](int){return hole;},{},true,0,{}, {},false,nullptr,&options);}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    int checks=0;refused=false;try{processing::drizzleProject(processing::linearRgb(a),{},64,64,options,[&]{if(++checks>5)throw std::runtime_error("cancel");});}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    bool cancelLast=false;refused=false;
    try{processing::integrate(2,[&](int){return a;},{},true,0,[&]{if(cancelLast)throw std::runtime_error("cancel");},[&](int n,int,const std::string& stage){if(n==1&&stage=="Drizzle projection")cancelLast=true;},false,nullptr,&options);}catch(const std::runtime_error&){refused=true;}
    ASTRO_TEST_CHECK(refused&&cancelLast);
    std::cout<<"OK\n";
}
