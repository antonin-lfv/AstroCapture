#include <astro/processing/DeepSky.hpp>
#include <random>
#include <cmath>
#include "Check.hpp"
using namespace astro;
recording::AstroImage stars(double angle=0,double dx=0,double dy=0,int seed=1) {
    recording::AstroImage image;image.width=256;image.height=224;image.exposure=60;image.channels=1;
    image.samples.resize(256*224);
    std::mt19937 random(seed);std::normal_distribution<float> noise(0,.0003);
    const std::array<std::array<double,2>,18> positions{{{25,31},{72,23},{124,42},{185,25},{220,58},{38,88},{94,75},{149,97},{202,110},{20,155},{68,140},{113,174},{175,160},{221,183},{47,196},{150,203},{198,202},{237,132}}};
    const double c=std::cos(angle),s=std::sin(angle);
    for(int y=0;y<224;++y)for(int x=0;x<256;++x) {
        double signal=.02+.000002*x;
        for(std::size_t n=0;n<positions.size();++n){const double sx=c*positions[n][0]-s*positions[n][1]+dx,sy=s*positions[n][0]+c*positions[n][1]+dy;const double r=(x-sx)*(x-sx)+(y-sy)*(y-sy);signal+=(.15+n*.007)*std::exp(-r/4.5);}
        image.samples[y*256+x]=float(signal)+noise(random);
    }return image;
}
int main(){
    auto a=stars(),b=stars(.025,4,-3,2);auto ar=processing::linearRgb(a),br=processing::linearRgb(b);
    const auto detected=processing::detectStars(ar);ASTRO_TEST_CHECK(detected.size()>=15);
    const auto aligned=processing::alignStars(ar,br);ASTRO_TEST_CHECK(aligned.matches>=12&&aligned.residual<.4);
    ASTRO_TEST_CHECK(std::abs(aligned.matrix[0]-std::cos(.025))<.002);
    ASTRO_TEST_CHECK(std::abs(aligned.matrix[1]-std::sin(.025))<.002);
    // Known flat attenuation and sensor pedestal: dark includes bias.
    recording::AstroImage signal;signal.width=8;signal.height=8;signal.channels=1;signal.exposure=60;signal.samples.assign(64,.4f);
    auto dark=signal,flat=signal,bias=signal;dark.samples.assign(64,.03f);bias.samples.assign(64,.01f);flat.samples.assign(64,1);
    for(int i=0;i<64;++i){flat.samples[i]=i%2?.8f:1.2f;signal.samples[i]=.4f*flat.samples[i]+.03f;}
    processing::Calibration calibration;calibration.dark=dark;calibration.bias=bias;calibration.flat=flat;
    const auto calibrated=processing::calibrate(signal,calibration);for(auto value:calibrated.samples)ASTRO_TEST_CHECK(std::abs(value-.4)<1e-6);
    auto hotDark=dark;hotDark.samples[27]=.8f;
    const auto badMap=processing::detectBadPixels(hotDark);ASTRO_TEST_CHECK(badMap.samples[27]==1 && badMap.samples[26]==0);
    calibration.badPixels=badMap;auto damaged=signal;damaged.samples[27]=2;
    const auto repaired=processing::calibrate(damaged,calibration);ASTRO_TEST_CHECK(std::abs(repaired.samples[27]-.4)<1e-5);
    calibration.dark->exposure=120;bool refused=false;try{processing::calibrate(signal,calibration);}catch(const std::exception&){refused=true;}ASTRO_TEST_CHECK(refused);
    auto otherDark=dark;dark.hasGain=otherDark.hasGain=true;dark.gain=10;otherDark.gain=20;
    refused=false;try{processing::masterFrame(2,[&](int n){return n?otherDark:dark;},false);}catch(const std::exception&){refused=true;}ASTRO_TEST_CHECK(refused);
    // Integration must reject a one-frame cosmic ray and discard an unusable field.
    std::vector<recording::AstroImage> frames;
    for(int n=0;n<5;++n)frames.push_back(stars(0,0,0,n+1));
    frames[2].samples[100*256+100]=2;
    auto blank=a;blank.samples.assign(blank.samples.size(),.02);frames.push_back(blank);
    const auto stacked=processing::integrate(int(frames.size()),[&](int n){return frames[n];},{},true,3);
    ASTRO_TEST_CHECK(stacked.accepted.size()==5&&stacked.rejected.size()==1&&stacked.totalExposure==300&&stacked.rejectedSamples>0);
    ASTRO_TEST_CHECK(stacked.image.width>=250&&stacked.image.height>=218);
    ASTRO_TEST_CHECK(*std::max_element(stacked.image.rgb.begin(),stacked.image.rgb.end())<.8f);
    const auto rotated=processing::integrate(2,[&](int n){return n?b:a;},{},true,3);
    ASTRO_TEST_CHECK(rotated.accepted.size()==2&&rotated.image.width<256&&rotated.image.height<224);
    for(auto value:rotated.image.rgb)ASTRO_TEST_CHECK(std::isfinite(value));
    processing::Finish finish;finish.stretch=false;auto unchanged=processing::finishDeepSky(ar,finish);ASTRO_TEST_CHECK(unchanged.rgb==ar.rgb);
    finish.stretch=true;finish.black=.01;finish.white=.4;finish.midtones=.15;
    const auto stretched=processing::finishDeepSky(ar,finish);ASTRO_TEST_CHECK(stretched.rgb[0]>ar.rgb[0]);ASTRO_TEST_CHECK(ar.rgb[0]==a.samples[0]);
    processing::FloatImage colors{2,1,{.2f,.1f,.05f,.0002f,.0001f,.00005f}};
    processing::Finish asinh;asinh.asinh=true;asinh.asinhStrength=20;
    const auto colorStretch=processing::finishDeepSky(colors,asinh);
    ASTRO_TEST_CHECK(std::abs(colorStretch.rgb[0]/colorStretch.rgb[1]-2)<1e-5&&colorStretch.rgb[0]>.2f);
    asinh.stretch=false;asinh.saturation=2;
    const auto saturated=processing::finishDeepSky(colors,asinh);
    ASTRO_TEST_CHECK(saturated.rgb[0]>colors.rgb[0]&&saturated.rgb[3]==colors.rgb[3]);
    finish.stretch=false;finish.background=true;const auto corrected=processing::finishDeepSky(ar,finish);ASTRO_TEST_CHECK(corrected.width==ar.width);for(auto v:corrected.rgb)ASTRO_TEST_CHECK(std::isfinite(v));
    // Recover independently curved backgrounds without clipping linear values.
    processing::FloatImage gradient{96,80,std::vector<float>(96*80*3)};
    for(int y=0;y<80;++y)for(int x=0;x<96;++x)for(int c=0;c<3;++c){double nx=(x+.5)/96.,ny=(y+.5)/80.;gradient.rgb[(y*96+x)*3+c]=float(.1+.02*(c+1)*nx+.03*ny*ny);}
    processing::BackgroundOptions bg;bg.degree=2;bg.samplesPerLine=8;bg.tolerance=10;
    for(int y=0;y<8;++y)for(int x=0;x<8;++x)bg.samples.push_back({(x+.5)/8.,(y+.5)/8.});
    const auto extraction=processing::extractBackground(gradient,bg);
    for(int c=0;c<3;++c){float low=1,high=-1;for(std::size_t i=c;i<extraction.image.rgb.size();i+=3){low=std::min(low,extraction.image.rgb[i]);high=std::max(high,extraction.image.rgb[i]);}ASTRO_TEST_CHECK(high-low<.001);}
    bg.samples={{.1,.1},{.2,.2},{.3,.3},{.4,.4},{.5,.5},{.6,.6}};refused=false;try{processing::extractBackground(gradient,bg);}catch(const std::exception&){refused=true;}ASTRO_TEST_CHECK(refused);
    processing::FloatImage grid{3,2,{1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6}};
    const auto quarter=processing::orientImage(grid,1,false,false);ASTRO_TEST_CHECK(quarter.width==2&&quarter.height==3&&quarter.rgb[0]==4&&quarter.rgb[3]==1);
    ASTRO_TEST_CHECK(processing::orientImage(quarter,3,false,false).rgb==grid.rgb);
    ASTRO_TEST_CHECK(processing::orientImage(processing::orientImage(grid,0,true,true),0,true,true).rgb==grid.rgb);
    processing::FloatImage green{1,1,{.2f,.8f,.4f}};
    const auto neutral=processing::removeGreen(green,0,1,false);ASTRO_TEST_CHECK(std::abs(neutral.rgb[1]-.3)<1e-6&&neutral.rgb[0]==.2f&&neutral.rgb[2]==.4f);
    auto lightness=[](const std::vector<float>& rgb){auto linear=[](double v){return v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);};const double y=.2126*linear(rgb[0])+.7152*linear(rgb[1])+.0722*linear(rgb[2]);return 116*(y>.008856?std::cbrt(y):7.787*y+16./116)-16;};
    processing::FloatImage gentle{1,1,{.3f,.4f,.35f}};const auto preserved=processing::removeGreen(gentle,0,1,true);ASTRO_TEST_CHECK(std::abs(lightness(gentle.rgb)-lightness(preserved.rgb))<.2);
    const auto noMask=processing::removeGreen(green,2,0,false);ASTRO_TEST_CHECK(noMask.rgb==green.rgb);
    const auto maxNeutral=processing::removeGreen(green,1,1,false);ASTRO_TEST_CHECK(maxNeutral.rgb[1]==.4f);
    refused=false;auto hdr=green;hdr.rgb[0]=2;try{processing::removeGreen(hdr);}catch(const std::exception&){refused=true;}ASTRO_TEST_CHECK(refused);
    processing::BackgroundOptions sequenceBg;
    const auto bgStack=processing::integrate(2,[&](int n){return frames[n];},{},true,3,{}, {},true,&sequenceBg);ASTRO_TEST_CHECK(bgStack.accepted.size()==2);
    const auto combined=processing::combineChannels(ar,br,ar);ASTRO_TEST_CHECK(combined.width>200&&combined.height>180);for(auto v:combined.rgb)ASTRO_TEST_CHECK(std::isfinite(v));
    int checks=0;refused=false;try{processing::integrate(5,[&](int n){return frames[n];},{},true,3,[&]{if(++checks>3)throw std::logic_error("cancel");});}catch(const std::logic_error&){refused=true;}ASTRO_TEST_CHECK(refused);
    std::cout<<"OK\n";
}
