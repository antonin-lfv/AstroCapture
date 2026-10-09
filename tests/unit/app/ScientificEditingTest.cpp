#include "DeepSkyController.hpp"
#include "DeepSkySettings.hpp"
#include "ImagePreview.hpp"
#include "Check.hpp"
#include <QGuiApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QFileInfo>
#include <cmath>
using namespace astro;
void wait(app::DeepSkyController& c){QElapsedTimer t;t.start();while(c.busy()&&t.elapsed()<20000){QCoreApplication::processEvents();QThread::msleep(1);}ASTRO_TEST_CHECK(!c.busy());}
int main(int argc,char** argv){
 QGuiApplication application(argc,argv);QTemporaryDir dir;app::DeepSkyController c;
 auto image=std::make_shared<processing::FloatImage>(processing::FloatImage{64,48,std::vector<float>(64*48*3)});
 for(int y=0;y<48;++y)for(int x=0;x<64;++x)for(int k=0;k<3;++k)image->rgb[(y*64+x)*3+k]=.15f+.001f*(x+y)+((x+y)%2?.01f:-.01f);
 auto snapshot=c.sessionSnapshot();snapshot.images={{"linear",image},{"finished",image}};const auto preview=app::makeImagePreview(*image);snapshot.previews={{"display",preview},{"result",preview},{"linear",preview}};snapshot.state["frames"]=QVariantList{QVariantMap{{"path",dir.filePath("source.fits")},{"role","Light"}}};snapshot.state["finish"]=app::defaultDeepSkySettings();c.restoreSession(snapshot);ASTRO_TEST_CHECK(c.hasResult());
 auto s=app::defaultDeepSkySettings();s["stretch"]=false;s["red"]=1.4;s["maskType"]=4;s["maskStrokes"]=QVariantList{QVariantMap{{"points",QVariantList{QVariant::fromValue(QVariantList{.5,.5})}},{"radius",.15},{"hardness",1.0},{"opacity",1.0},{"value",0.0}}};
 ASTRO_TEST_CHECK(app::checkedDeepSkySettings(s));c.apply(s);wait(c);const auto masked=c.sessionSnapshot().images["finished"];
 const auto center=(24*64+32)*3;ASTRO_TEST_CHECK(masked->rgb[center]==image->rgb[center]);ASTRO_TEST_CHECK(std::abs(masked->rgb[0]-image->rgb[0]*1.4f)<1e-6);
 s["maskStrokes"]=QVariantList{};s["maskBaseWhite"]=false;s["maskScope"]=1;s["advancedDenoise"]=true;
 c.apply(s);wait(c);const auto noiseOnly=c.sessionSnapshot().images["finished"];
 for(std::size_t p=0;p<image->rgb.size();++p)ASTRO_TEST_CHECK(std::abs(noiseOnly->rgb[p]-image->rgb[p]*(p%3==0?1.4f:1.f))<1e-6);
 s["maskScope"]=2;s["advancedDenoise"]=false;s["deconvolution"]=true;s["rlIterations"]=2;
 c.apply(s);wait(c);const auto rlOnly=c.sessionSnapshot().images["finished"];
 for(std::size_t p=0;p<image->rgb.size();++p)ASTRO_TEST_CHECK(std::abs(rlOnly->rgb[p]-image->rgb[p]*(p%3==0?1.4f:1.f))<1e-6);
 s["deconvolution"]=false;s["maskScope"]=3;c.apply(s);wait(c);ASTRO_TEST_CHECK(c.sessionSnapshot().images["finished"]->rgb==image->rgb);
 QImage mask(64,48,QImage::Format_RGBA64);for(int y=0;y<48;++y){auto* row=reinterpret_cast<QRgba64*>(mask.scanLine(y));for(int x=0;x<64;++x){const quint16 v=x<32?0:65535;row[x]=QRgba64::fromRgba64(v,v,v,65535);}}
 const auto path=dir.filePath("mask.png");ASTRO_TEST_CHECK(mask.save(path));c.loadMaskFile(QUrl::fromLocalFile(path));wait(c);ASTRO_TEST_CHECK(c.importedMaskPath()==path&&!c.importedMaskImage().isNull());
 s["maskType"]=5;s["maskFile"]=path;s["maskScope"]=0;c.apply(s);wait(c);const auto imported=c.sessionSnapshot().images["finished"];ASTRO_TEST_CHECK(imported->rgb[0]==image->rgb[0]);ASTRO_TEST_CHECK(std::abs(imported->rgb[63*3]-image->rgb[63*3]*1.4f)<1e-6);
 auto geometry=app::defaultGeometry();geometry["crop"]=true;geometry["x"]=5;geometry["y"]=4;geometry["width"]=30;geometry["height"]=20;s["geometry"]=geometry;c.apply(s);wait(c);ASTRO_TEST_CHECK(c.imageWidth()==30&&c.imageHeight()==20&&c.comparisonImage().size()==QSize(30,20)&&c.geometryImage().size()==QSize(64,48));
 const auto good=c.sessionSnapshot().images["finished"];s["maskFile"]=dir.filePath("gone.png");c.apply(s);wait(c);ASTRO_TEST_CHECK(c.sessionSnapshot().images["finished"]==good);
 geometry["width"]=100;s["maskType"]=0;s["geometry"]=geometry;c.apply(s);wait(c);ASTRO_TEST_CHECK(c.sessionSnapshot().images["finished"]==good);
}
