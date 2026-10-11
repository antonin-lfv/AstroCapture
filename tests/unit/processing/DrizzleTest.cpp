#include <astro/processing/DeepSky.hpp>
#include <astro/processing/WcsProjection.hpp>
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
recording::AstroImage bayerField(double dx=0,double dy=0,domain::BayerPattern pattern=domain::BayerPattern::RGGB) {
    auto image=field(dx,dy);const auto rgb=std::move(image.samples);image.channels=1;image.bayer=pattern;image.samples.resize(64*64);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x){const auto p=y*64+x;image.samples[p]=rgb[p*3+domain::bayerChannelAt(pattern,x,y)];}
    return image;
}
}
int main() {
    // Each CFA sample remains confined to its measured color, including real
    // zeros, negative values and HDR. Unknown layouts cannot silently debayer.
    for(const auto pattern:{domain::BayerPattern::RGGB,domain::BayerPattern::BGGR,domain::BayerPattern::GRBG,domain::BayerPattern::GBRG}) {
        recording::AstroImage raw;raw.width=raw.height=16;raw.bayer=pattern;raw.samples.resize(256);
        const std::array<float,3> levels{-.02f,0.f,2.25f};
        for(int y=0;y<16;++y)for(int x=0;x<16;++x)raw.samples[y*16+x]=levels[domain::bayerChannelAt(pattern,x,y)];
        auto p=processing::drizzleProjectBayer(raw,{},16,16,{1,1,true});
        ASTRO_TEST_CHECK(p.weightChannels==3&&p.weights.size()==256*3);
        for(int y=0;y<16;++y)for(int x=0;x<16;++x)for(int c=0;c<3;++c){const auto at=(y*16+x)*3+c;const bool measured=c==domain::bayerChannelAt(pattern,x,y);ASTRO_TEST_CHECK(p.weights[at]==(measured?1.f:0.f));ASTRO_TEST_CHECK(measured?p.image.rgb[at]==levels[c]:std::isnan(p.image.rgb[at]));}
        // Four integer phases of the 2×2 CFA tile reconstruct independent
        // high-frequency color fields. Conventional debayering smooths them.
        auto colorScene=[](int c,double x,double y){return (c==0?-.4:c==1?0.:2.25)+.15*std::sin((2.3-c*.2)*x)*std::cos((1.9+c*.15)*y);};
        std::vector<double> sums(256*3),weights(256*3),blurred(256*3),blurWeights(256);
        for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
            for(int y=0;y<16;++y)for(int x=0;x<16;++x)raw.samples[y*16+x]=float(colorScene(domain::bayerChannelAt(pattern,x,y),x+dx,y+dy));
            processing::StarAlignment alignment;alignment.matrix[2]=dx;alignment.matrix[5]=dy;
            auto projected=processing::drizzleProjectBayer(raw,alignment,16,16,{1,1,true});
            const auto debayered=processing::drizzleProject(processing::linearRgb(raw),alignment,16,16,{1,1});
            for(int at=0;at<256*3;++at){if(projected.weights[at]>0){sums[at]+=projected.image.rgb[at]*projected.weights[at];weights[at]+=projected.weights[at];}if(debayered.weights[at/3]>0)blurred[at]+=debayered.image.rgb[at]*debayered.weights[at/3];}
            for(int at=0;at<256;++at)blurWeights[at]+=debayered.weights[at];
        }
        double error=0,blurError=0;
        for(int y=2;y<14;++y)for(int x=2;x<14;++x)for(int c=0;c<3;++c){const int at=(y*16+x)*3+c;ASTRO_TEST_CHECK(weights[at]>0);const double truth=colorScene(c,x,y);error+=std::pow(sums[at]/weights[at]-truth,2);blurError+=std::pow(blurred[at]/blurWeights[at/3]-truth,2);}
        ASTRO_TEST_CHECK(error<1e-10&&blurError>.1);
    }
    // A rotated isolated red sample contributes a unit area to red alone.
    {
        // Sixteen phases of the CFA tile cover a ×2 grid with half-size
        // drops, preserving independent analytic color signals at each site.
        recording::AstroImage raw;raw.width=raw.height=16;raw.bayer=domain::BayerPattern::RGGB;raw.samples.resize(256);
        std::vector<double> sums(32*32*3),weights(sums.size());
        auto scene=[](int c,double x,double y){return c+.1*std::sin(2.2*x)*std::cos(2.5*y);};
        for(double dy:{-.25,.25,.75,1.25})for(double dx:{-.25,.25,.75,1.25}){
            for(int y=0;y<16;++y)for(int x=0;x<16;++x)raw.samples[y*16+x]=float(scene(domain::bayerChannelAt(raw.bayer,x,y),x+dx,y+dy));
            processing::StarAlignment alignment;alignment.matrix[2]=dx;alignment.matrix[5]=dy;
            auto projected=processing::drizzleProjectBayer(raw,alignment,16,16,{2,.5,true});
            for(std::size_t at=0;at<sums.size();++at)if(projected.weights[at]>0){sums[at]+=projected.image.rgb[at]*projected.weights[at];weights[at]+=projected.weights[at];}
        }
        for(int y=4;y<28;++y)for(int x=4;x<28;++x)for(int c=0;c<3;++c){const int at=(y*32+x)*3+c;ASTRO_TEST_CHECK(weights[at]>0&&std::abs(sums[at]/weights[at]-scene(c,(x+.5)/2-.5,(y+.5)/2-.5))<2e-7);}
    }
    auto single=bayerField();single.samples.assign(4096,NAN);single.samples[32*64+32]=3.5f;
    processing::StarAlignment cfaRotation;const double cosine=std::sqrt(.5);cfaRotation.matrix={cosine,-cosine,32,cosine,cosine,32-64*cosine};
    const auto cfaRotated=processing::drizzleProjectBayer(single,cfaRotation,64,64,{2,.8,true});
    std::array<double,3> channelWeights{};for(std::size_t p=0;p<cfaRotated.weights.size()/3;++p)for(int c=0;c<3;++c){const auto at=p*3+c;channelWeights[c]+=cfaRotated.weights[at];if(cfaRotated.weights[at]>0)ASTRO_TEST_CHECK(c==0&&cfaRotated.image.rgb[at]==3.5f);}
    ASTRO_TEST_CHECK(std::abs(channelWeights[0]-1)<1e-6&&channelWeights[1]==0&&channelWeights[2]==0);

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
    for(const auto matrix:std::array<std::array<double,6>,4>{{{1,0,.37,0,1,-.2},{.8,-.3,4,.3,.8,2},{0,-1,7,1,0,0},{1,.2,-2,.1,1,-1}}}) {
        processing::StarAlignment m;m.matrix=matrix;
        auto source=processing::linearRgb(field());
        const auto full=processing::drizzleProject(source,m,64,64,{2,.8});
        std::vector<float> rgb(full.image.rgb.size()),weights(full.weights.size());
        processing::drizzleProjectStrips(source,m,64,64,{2,.8},3,[&](int y,const auto& tile){std::copy(tile.image.rgb.begin(),tile.image.rgb.end(),rgb.begin()+std::size_t(y)*128*3);std::copy(tile.weights.begin(),tile.weights.end(),weights.begin()+std::size_t(y)*128);});
        ASTRO_TEST_CHECK(weights==full.weights);
        for(std::size_t i=0;i<rgb.size();++i)ASTRO_TEST_CHECK(rgb[i]==full.image.rgb[i]||(std::isnan(rgb[i])&&std::isnan(full.image.rgb[i])));
        const auto raw=bayerField();const auto fullCfa=processing::drizzleProjectBayer(raw,m,64,64,{2,.8,true});
        weights.resize(fullCfa.weights.size());
        processing::drizzleProjectBayerStrips(raw,m,64,64,{2,.8,true},1,[&](int y,const auto& tile){std::copy(tile.image.rgb.begin(),tile.image.rgb.end(),rgb.begin()+std::size_t(y)*128*3);std::copy(tile.weights.begin(),tile.weights.end(),weights.begin()+std::size_t(y)*128*3);});
        ASTRO_TEST_CHECK(weights==fullCfa.weights);
        for(std::size_t i=0;i<rgb.size();++i)ASTRO_TEST_CHECK(rgb[i]==fullCfa.image.rgb[i]||(std::isnan(rgb[i])&&std::isnan(fullCfa.image.rgb[i])));
    }
    auto a=field();recording::CelestialWcs wcs;wcs.crval={23,30};wcs.crpix={32.5,32.5};wcs.cd={-.001,0,0,.001};wcs.sipOrder=3;wcs.sipA[12]=1e-5;wcs.sipB[2]=-2e-5;a.wcs=wcs;a.pixelScale=3.6;
    {
        auto target=wcs;target.sipOrder=0;target.sipA.fill(0);target.sipB.fill(0);
        processing::WcsProjection projection(wcs,target,64,64);const auto source=processing::linearRgb(a);
        const auto full=processing::drizzleProject(source,{},64,64,{2,.8},{},&projection);
        std::vector<float> rgb(full.image.rgb.size()),weights(full.weights.size());
        processing::drizzleProjectStrips(source,{},64,64,{2,.8},2,[&](int y,const auto& tile){std::copy(tile.image.rgb.begin(),tile.image.rgb.end(),rgb.begin()+std::size_t(y)*128*3);std::copy(tile.weights.begin(),tile.weights.end(),weights.begin()+std::size_t(y)*128);},{},&projection);
        ASTRO_TEST_CHECK(weights==full.weights);
        for(std::size_t i=0;i<rgb.size();++i)ASTRO_TEST_CHECK(rgb[i]==full.image.rgb[i]||(std::isnan(rgb[i])&&std::isnan(full.image.rgb[i])));
    }
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
    const auto large=processing::drizzleDimensions(6244,4170,{2,.8});ASTRO_TEST_CHECK(large[0]==12488&&large[1]==8340);
    bool refused=false;try{processing::drizzleDimensions(8000,5000,{2,.8});}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    refused=false;try{processing::drizzleProject(input,{},8,8,{1,0});}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    refused=false;try{processing::integrate(1,[&](int){return a;},{},true,0,{}, {},false,nullptr,&options);}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    auto hole=a;for(int y=25;y<30;++y)for(int x=25;x<30;++x)for(int ch=0;ch<3;++ch)hole.samples[(y*64+x)*3+ch]=NAN;
    refused=false;try{processing::integrate(2,[&](int){return hole;},{},true,0,{}, {},false,nullptr,&options);}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    int checks=0;refused=false;try{processing::drizzleProject(processing::linearRgb(a),{},64,64,options,[&]{if(++checks>5)throw std::runtime_error("cancel");});}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    bool cancelLast=false;refused=false;
    try{processing::integrate(2,[&](int){return a;},{},true,0,[&]{if(cancelLast)throw std::runtime_error("cancel");},[&](int n,int,const std::string& stage){if(n==1&&stage=="Drizzle projection")cancelLast=true;},false,nullptr,&options);}catch(const std::runtime_error&){refused=true;}
    ASTRO_TEST_CHECK(refused&&cancelLast);
    processing::DrizzleOptions cfaOptions{1,1,true};
    refused=false;try{processing::integrate(2,[&](int){return a;},{},true,0,{}, {},false,nullptr,&cfaOptions);}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    refused=false;try{processing::integrate(2,[&](int){return bayerField();},{},true,0,{}, {},false,nullptr,&cfaOptions);}catch(const std::runtime_error& e){refused=std::string(e.what()).find("uncovered color")!=std::string::npos;}ASTRO_TEST_CHECK(refused);
    // Native calibration (bias then flat), per-color normalization, WCS and
    // disk-backed RGB weights all pass through the real integration pipeline.
    for(bool normalize:{false,true}){
        processing::Calibration masters;auto bias=bayerField();bias.samples.assign(4096,.001f);bias.exposure=0;masters.bias=bias;auto flat=bias;flat.samples.assign(4096,.8f);masters.flat=flat;
        auto result=processing::integrate(12,[&](int n){auto raw=bayerField(n%2,(n/2)%2);raw.wcs=wcs;raw.pixelScale=3.6;for(auto& v:raw.samples)v=v*.8f+.001f;if(n==8)raw.samples[36*64+36]=20;return raw;},masters,false,3,{}, {},normalize,nullptr,&cfaOptions);
        ASTRO_TEST_CHECK(result.nativeBayer&&result.accepted.size()==12&&result.minimumContributors>=3&&result.meanContributors>3&&result.subpixelDiversity&&result.rejectedSamples>0&&result.wcs&&std::abs(result.wcs->scale()-3.6)<1e-8);
        for(float v:result.image.rgb)ASTRO_TEST_CHECK(std::isfinite(v));
        processing::IntegrationLimits tiny;tiny.stripBytes=12*24*3;
        auto tiled=processing::integrate(12,[&](int n){auto raw=bayerField(n%2,(n/2)%2);raw.wcs=wcs;raw.pixelScale=3.6;for(auto& v:raw.samples)v=v*.8f+.001f;if(n==8)raw.samples[36*64+36]=20;return raw;},masters,false,3,{}, {},normalize,nullptr,&cfaOptions,false,false,&tiny);
        ASTRO_TEST_CHECK(tiled.image.rgb==result.image.rgb && tiled.accepted==result.accepted && tiled.rejectedSamples==result.rejectedSamples);
        ASTRO_TEST_CHECK(result.image.width>=60&&result.image.height>=60);
        std::array<std::vector<float>,3> background;for(int y=2;y<7;++y)for(int x=2;x<7;++x)for(int c=0;c<3;++c)background[c].push_back(result.image.rgb[(y*result.image.width+x)*3+c]);for(int c=0;c<3;++c){std::sort(background[c].begin(),background[c].end());ASTRO_TEST_CHECK(std::abs(background[c][12]-.02*(c+1))<.001);}
    }
    // CFA cancellation must propagate even during the final projection.
    bool cancelCfa=false;refused=false;try{processing::integrate(4,[&](int n){return bayerField(n%2,n/2);},{},true,0,[&]{if(cancelCfa)throw std::runtime_error("cancel");},[&](int n,int,const std::string& stage){if(n==3&&stage=="Drizzle projection")cancelCfa=true;},false,nullptr,&cfaOptions);}catch(const std::runtime_error&){refused=true;}ASTRO_TEST_CHECK(refused&&cancelCfa);
    std::cout<<"OK\n";
}
