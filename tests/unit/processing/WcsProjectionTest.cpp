#include <astro/processing/WcsProjection.hpp>
#include <astro/recording/AstroImage.hpp>
#include "Check.hpp"
#include <cmath>
#include <limits>
#include <numeric>
#include <filesystem>
using namespace astro;
namespace {
using Wcs=recording::CelestialWcs;
Wcs base(){Wcs w;w.crval={359.98,62};w.crpix={80.5,80.5};w.cd={-.002,0,0,.002};return w;}
Wcs distorted(int n=0){auto w=base();w.crpix[0]+=n*.6;w.crpix[1]-=n*.4;w.sipOrder=3;w.sipA[12]=2e-4;w.sipB[2]=-1e-4;w.sipA[18]=n*1.5e-6;w.sipB[8]=n*1e-6;return w;}
std::array<double,2> independent(const Wcs& a,const Wcs& b,double x,double y){const auto sky=a.sky(x,y);ASTRO_TEST_CHECK(sky);const auto p=b.pixel((*sky)[0],(*sky)[1]);ASTRO_TEST_CHECK(p);return *p;}
recording::AstroImage field(const Wcs& w) {
    recording::AstroImage image;image.width=image.height=160;image.channels=3;image.bits=32;image.exposure=60;image.wcs=w;image.samples.resize(160*160*3);
    // Independent celestial-coordinate evaluation, not the new fast mapper.
    for(int y=0;y<160;++y)for(int x=0;x<160;++x){const auto p=independent(w,base(),x,y);double value=.02;for(int iy=0;iy<4;++iy)for(int ix=0;ix<4;++ix){const double sx=20+ix*36+(iy%2)*3,sy=20+iy*36;value+=(.16+.03*ix)*std::exp(-((p[0]-sx)*(p[0]-sx)+(p[1]-sy)*(p[1]-sy))/6.);}
        for(int c=0;c<3;++c)image.samples[(y*160+x)*3+c]=float(value*(1-c*.3));
    }return image;
}
template<class F>void fails(F f){bool threw=false;try{f();}catch(const std::runtime_error&){threw=true;}ASTRO_TEST_CHECK(threw);}
}
int main() {
    // Exact different TAN centers, RA wrap, parity, rotation, SIP order 5,
    // and near-polar fields compared with the spherical WCS implementation.
    for(int n=0;n<4;++n){auto a=distorted(1),b=base();b.crval[0]=.025;b.crval[1]+=.015;if(n==1)b.cd={0,.002,.002,0};if(n==2){a.crval[1]=b.crval[1]=89.8;a.crval[0]=.02;b.crval[0]=.06;}if(n==3){a.sipOrder=5;a.sipA[30]=1e-11;a.sipB[5]=-1e-11;b=distorted(0);}
        processing::WcsProjection p(a,b,160,160);for(int y=0;y<=160;y+=8)for(int x=0;x<=160;x+=8){const auto expected=independent(a,b,x,y),v=p.forward(x,y),back=p.inverse(v[0],v[1]);ASTRO_TEST_CHECK(std::hypot(v[0]-expected[0],v[1]-expected[1])<1e-6);ASTRO_TEST_CHECK(std::hypot(back[0]-x,back[1]-y)<1e-6);}
    }
    auto a=field(distorted()),b=field(distorted(1));processing::WcsProjection relative(*b.wcs,*a.wcs,160,160);
    auto registration=processing::validateWcsRegistration(processing::linearRgb(a),processing::linearRgb(b),relative);ASTRO_TEST_CHECK(registration.matches>=12&&registration.residual<.1);
    auto integration=processing::integrate(2,[&](int n){return n?b:a;},{},true,0,{}, {},false,nullptr,nullptr,true,true);
    ASTRO_TEST_CHECK(integration.wcsProjection&&integration.wcsChecks.size()==2&&integration.wcs&&integration.wcs->sipOrder==0&&integration.accepted.size()==2);
    auto truth=field(base());double error=0;int samples=0;
    for(int y=3;y<integration.image.height-3;++y)for(int x=3;x<integration.image.width-3;++x){const auto xy=independent(*integration.wcs,base(),x,y);const int xx=std::lround(xy[0]),yy=std::lround(xy[1]);ASTRO_TEST_CHECK(std::hypot(xy[0]-xx,xy[1]-yy)<1e-6);for(int c=0;c<3;++c){const auto at=(y*integration.image.width+x)*3+c;error+=std::pow(integration.image.rgb[at]-truth.samples[(yy*160+xx)*3+c],2);++samples;ASTRO_TEST_CHECK(integration.contributors.rgb[at]==2&&integration.weights.rgb[at]==2);}}
    ASTRO_TEST_CHECK(std::sqrt(error/samples)<.002);
    // Drizzle and normal stacking both publish undistorted coordinates. Maps
    // share that grid and no nonlinear transform is silently replaced.
    processing::DrizzleOptions drizzle{1.5,1};auto drizzled=processing::integrate(2,[&](int n){return n?b:a;},{},true,0,{}, {},false,nullptr,&drizzle,true,true);
    ASTRO_TEST_CHECK(drizzled.wcs->sipOrder==0&&std::abs(drizzled.wcs->scale()-base().scale()/1.5)<1e-8&&drizzled.wcsChecks.size()==2);
    for(auto v:drizzled.image.rgb)ASTRO_TEST_CHECK(std::isfinite(v));
    processing::DrizzleOptions native{1,1,true};auto cfa=processing::integrate(8,[&](int n){auto w=distorted();w.crpix[0]+=n%2;w.crpix[1]+=(n/2)%2;auto data=field(w);const auto rgb=std::move(data.samples);data.channels=1;data.bayer=domain::BayerPattern::RGGB;data.samples.resize(160*160);for(int y=0;y<160;++y)for(int x=0;x<160;++x)data.samples[y*160+x]=rgb[(y*160+x)*3+domain::bayerChannelAt(data.bayer,x,y)];return data;},{},true,0,{}, {},false,nullptr,&native,true,true);
    ASTRO_TEST_CHECK(cfa.nativeBayer&&cfa.wcsProjection&&cfa.wcsChecks.size()==8&&cfa.wcs->sipOrder==0&&cfa.minimumContributors>=1);for(auto v:cfa.image.rgb)ASTRO_TEST_CHECK(std::isfinite(v));
    // An isolated curved drop retains unit total geometric weight. Compare
    // output-pixel overlaps against independent dense quadrature weighted by
    // the analytically known SIP Jacobian.
    auto w=base();w.sipOrder=2;w.sipA[12]=.002;w.sipB[2]=.001;processing::WcsProjection curve(w,base(),160,160);
    processing::FloatImage sparse{160,160,std::vector<float>(160*160*3,NAN)};for(int c=0;c<3;++c)sparse.rgb[(50*160+110)*3+c]=c==0?-.1f:c==1?0.f:2.5f;
    const auto projected=processing::drizzleProject(sparse,{},160,160,{1,1},{},&curve);
    ASTRO_TEST_CHECK(std::abs(std::accumulate(projected.weights.begin(),projected.weights.end(),0.)-1)<1e-6);
    std::vector<double> oracle(160*160);double total=0;constexpr int steps=400;
    for(int iy=0;iy<steps;++iy)for(int ix=0;ix<steps;++ix){const double x=109.5+(ix+.5)/steps,y=49.5+(iy+.5)/steps;const auto p=independent(w,base(),x,y);const double jac=(1+.004*(x+1-w.crpix[0]))*(1+.002*(y+1-w.crpix[1]));const int xx=int(std::floor(p[0]+.5)),yy=int(std::floor(p[1]+.5));oracle[yy*160+xx]+=jac;total+=jac;}
    for(std::size_t p=0;p<oracle.size();++p){ASTRO_TEST_CHECK(std::abs(projected.weights[p]-oracle[p]/total)<.003);if(projected.weights[p]>0){ASTRO_TEST_CHECK(std::abs(projected.image.rgb[p*3]+.1)<1e-7&&projected.image.rgb[p*3+1]==0&&projected.image.rgb[p*3+2]==2.5f);}}
    for(auto pattern:{domain::BayerPattern::RGGB,domain::BayerPattern::BGGR,domain::BayerPattern::GRBG,domain::BayerPattern::GBRG}) {
        auto raw=a;raw.channels=1;raw.bayer=pattern;raw.samples.assign(160*160,NAN);raw.samples[50*160+110]=2.5f;
        const auto cfa=processing::drizzleProjectBayer(raw,{},160,160,{1,1,true},{},&curve);const int channel=domain::bayerChannelAt(pattern,110,50);double weight=0;
        for(std::size_t p=0;p<cfa.weights.size()/3;++p)for(int c=0;c<3;++c){if(c==channel){weight+=cfa.weights[p*3+c];if(cfa.weights[p*3+c]>0)ASTRO_TEST_CHECK(cfa.image.rgb[p*3+c]==2.5f);}else ASTRO_TEST_CHECK(cfa.weights[p*3+c]==0&&std::isnan(cfa.image.rgb[p*3+c]));}ASTRO_TEST_CHECK(std::abs(weight-1)<1e-6);
    }
    // Wrong metadata, absent WCS, folds, too few / clustered stars, and
    // cancellation are explicit failures; a later bad source is reported.
    auto bad=b;bad.wcs->crpix[0]+=12;
    fails([&]{processing::integrate(2,[&](int n){return n?bad:a;},{},true,0,{}, {},false,nullptr,nullptr,false,true);});
    auto accepted=processing::integrate(3,[&](int n){return n==2?bad:n?b:a;},{},true,0,{}, {},false,nullptr,nullptr,false,true);ASTRO_TEST_CHECK(accepted.accepted.size()==2&&accepted.rejected==std::vector<int>{2}&&accepted.wcsChecks.size()==2);
    bad=b;bad.wcs.reset();fails([&]{processing::integrate(2,[&](int n){return n?bad:a;},{},true,0,{}, {},false,nullptr,nullptr,false,true);});
    auto fold=base();fold.sipOrder=3;fold.sipA[18]=-.0002;fails([&]{processing::WcsProjection p(fold,base(),160,160);});
    auto empty=processing::linearRgb(b);empty.rgb.assign(empty.rgb.size(),.02);fails([&]{processing::validateWcsRegistration(processing::linearRgb(a),empty,relative);});
    fails([&]{processing::WcsProjection p(base(),base(),160,160,[]{throw std::runtime_error("cancel");});});
    fails([&]{processing::drizzleProject(sparse,{},160,160,{1,1},[]{throw std::runtime_error("cancel");},&curve);});
    fails([&]{processing::reprojectWcs(processing::linearRgb(a),curve,160,160,[]{throw std::runtime_error("cancel");});});
    const auto path=std::filesystem::temp_directory_path()/"astro-wcs-projection-test.fits";recording::AstroImage output;output.width=integration.image.width;output.height=integration.image.height;output.channels=3;output.bits=32;output.samples=integration.image.rgb;output.wcs=integration.wcs;recording::writeAstroFits(path,output);const auto restored=recording::readAstroImage(path);std::filesystem::remove(path);ASTRO_TEST_CHECK(restored.wcs&&restored.wcs->sipOrder==0&&restored.samples==output.samples);
}
