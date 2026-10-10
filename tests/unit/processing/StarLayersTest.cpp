#include "Check.hpp"
#include <astro/processing/StarLayers.hpp>
#include <cmath>
#include <stop_token>
using namespace astro;
int main(){processing::FloatImage image{256,256,std::vector<float>(256*256*3)};const std::array<std::array<double,2>,5> centers{{{40,40},{100,45},{200,55},{55,125},{200,200}}};for(int y=0;y<256;++y)for(int x=0;x<256;++x)for(int k=0;k<3;++k){double v=.02+.00003*x+.00002*y+.12*std::exp(-(std::pow(x-125.,2)+std::pow(y-165.,2))/(2*25*25));for(const auto& s:centers)v+=(k==0?.4:k==1?.3:.2)*std::exp(-4*std::log(2.)*(std::pow(x-s[0],2)+std::pow(y-s[1],2))/9);image.rgb[(y*256+x)*3+k]=v;}
 processing::StarSeparationOptions options;auto layers=processing::separateStars(image,options);ASTRO_TEST_CHECK(layers.accepted>=4);for(std::size_t i=0;i<image.rgb.size();++i)ASTRO_TEST_CHECK(std::abs((layers.background.rgb[i]+layers.stars.rgb[i])-image.rgb[i])<1e-7);const auto nebula=(165*256+125)*3;ASTRO_TEST_CHECK(layers.background.rgb[nebula]==image.rgb[nebula]);const auto star=(40*256+40)*3;ASTRO_TEST_CHECK(layers.stars.rgb[star]>.35&&layers.background.rgb[star]<.04);
 processing::Finish common;common.black=.015;common.white=.8;common.midtones=.12;const auto normal=processing::finishDeepSky(image,common);auto neutral=processing::finishStarLayers(image,options,common,{});ASTRO_TEST_CHECK(neutral.image.rgb==normal.rgb);
 processing::StarLayerAdjustments reduce;reduce.starGain=.4;const auto smaller=processing::finishStarLayers(image,options,common,reduce);ASTRO_TEST_CHECK(smaller.image.rgb[star]<neutral.image.rgb[star]&&smaller.image.rgb[nebula]==neutral.image.rgb[nebula]);for(std::size_t i=0;i<image.rgb.size();++i)ASTRO_TEST_CHECK(std::abs(smaller.image.rgb[i]-(smaller.background.rgb[i]+smaller.stars.rgb[i]))<1e-7);
 auto bright=image;bright.rgb[star]=1;const auto saturated=processing::separateStars(bright,options);ASTRO_TEST_CHECK(saturated.saturated>=1&&saturated.stars.rgb[star]==0&&saturated.background.rgb[star]==1);
 bool refused=false;std::stop_source stop;stop.request_stop();try{processing::separateStars(image,options,[&]{if(stop.stop_requested())throw std::runtime_error("cancel");});}catch(const std::exception&){refused=true;}ASTRO_TEST_CHECK(refused);
 // Pure extended emission is not a stellar layer.
 auto smooth=image;for(int y=0;y<256;++y)for(int x=0;x<256;++x)for(int k=0;k<3;++k)smooth.rgb[(y*256+x)*3+k]=.03+.2*std::exp(-(std::pow(x-128.,2)+std::pow(y-128.,2))/(2*25*25));const auto extended=processing::separateStars(smooth,options);ASTRO_TEST_CHECK(extended.accepted==0);
}
