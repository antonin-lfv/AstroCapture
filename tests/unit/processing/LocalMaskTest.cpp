#include <astro/processing/DeepSkyAdvanced.hpp>
#include "Check.hpp"
#include <cmath>
using namespace astro::processing;
int main(){
 FloatImage f{80,40,std::vector<float>(80*40*3,.5f)};MaskOptions m;m.type=4;
 MaskStroke stroke;stroke.points={{.2,.5},{.5,.5},{.8,.5}};stroke.radius=.1;stroke.hardness=1;stroke.opacity=.5;m.strokes={stroke};
 const auto mask=makeMask(f,m);ASTRO_TEST_CHECK(mask[20*80+40]==.5f&&mask[0]==1.f&&mask[10*80+40]==1.f);
 m.strokes.push_back(stroke);ASTRO_TEST_CHECK(makeMask(f,m)[20*80+40]==.25f);
 m.invert=true;const auto inverse=makeMask(f,m);ASTRO_TEST_CHECK(inverse[0]==0.f&&inverse[20*80+40]==.75f);
 m.invert=false;m.blurRadius=2;const auto soft=makeMask(f,m);ASTRO_TEST_CHECK(soft[16*80+40]>.25f&&soft[16*80+40]<1.f);for(float v:soft)ASTRO_TEST_CHECK(v>=0&&v<=1);
 m={};m.type=5;m.raster.resize(80*40);for(std::size_t i=0;i<m.raster.size();++i)m.raster[i]=float(i)/m.raster.size();ASTRO_TEST_CHECK(makeMask(f,m)==m.raster);
 m.raster[0]=2;bool rejected=false;try{makeMask(f,m);}catch(...){rejected=true;}ASTRO_TEST_CHECK(rejected);
 m.type=4;m.raster.clear();m.strokes={stroke};m.strokes[0].points[0][0]=2;rejected=false;try{makeMask(f,m);}catch(...){rejected=true;}ASTRO_TEST_CHECK(rejected);
}
