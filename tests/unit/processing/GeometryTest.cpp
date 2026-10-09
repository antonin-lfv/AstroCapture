#include <astro/processing/Geometry.hpp>
#include <astro/processing/DeepSky.hpp>
#include "Check.hpp"
#include <cmath>
#include <stdexcept>
using namespace astro::processing;
int main(){
 FloatImage f{9,7,std::vector<float>(9*7*3)};for(std::size_t i=0;i<f.rgb.size();++i)f.rgb[i]=float(i)*.031f-2.f;
 Geometry g;g.crop=true;g.x=2;g.y=1;g.width=4;g.height=3;
 const auto crop=transformGeometry(f,g);ASTRO_TEST_CHECK(crop.width==4&&crop.height==3);
 for(int y=0;y<3;++y)for(int x=0;x<4;++x)for(int c=0;c<3;++c)ASTRO_TEST_CHECK(crop.rgb[(y*4+x)*3+c]==f.rgb[((y+1)*9+x+2)*3+c]);
 g.width=8;bool rejected=false;try{transformGeometry(f,g);}catch(const std::exception&){rejected=true;}ASTRO_TEST_CHECK(rejected);
 g={};g.angle=90;g.interpolation=0;const auto rotation=transformGeometry(f,g);const auto exact=orientImage(f,1,false,false);ASTRO_TEST_CHECK(rotation.width==7&&rotation.height==9&&rotation.rgb==exact.rgb);
 g.angle=-90;const auto counter=transformGeometry(f,g);ASTRO_TEST_CHECK(counter.rgb==orientImage(f,3,false,false).rgb);
 for(int mode=0;mode<5;++mode){g={};g.interpolation=mode;g.scale=2;const auto resized=transformGeometry(f,g);ASTRO_TEST_CHECK(resized.width==18&&resized.height==14);for(float v:resized.rgb)ASTRO_TEST_CHECK(std::isfinite(v)&&v>=f.rgb.front()&&v<=f.rgb.back());}
 g={};g.angle=34;g.keepSize=true;const auto tilted=transformGeometry(f,g);ASTRO_TEST_CHECK(tilted.width==9&&tilted.height==7);
 bool cancelled=false;try{transformGeometry(f,{},[]{throw std::runtime_error("cancel");});}catch(...){cancelled=true;}ASTRO_TEST_CHECK(cancelled);
 ASTRO_TEST_CHECK(transformGeometry(f,{}).rgb==f.rgb);
}
