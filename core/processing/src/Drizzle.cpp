#include <astro/domain/ImageLimits.hpp>
#include <astro/processing/DeepSky.hpp>
#include <astro/processing/WcsProjection.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace astro::processing {
namespace {
struct Point { double x,y; };
struct Polygon { std::array<Point,12> points{}; int size{}; };
Polygon clip(const Polygon& input,int axis,double edge,bool lower) {
    Polygon output;if(!input.size)return output;
    auto coordinate=[axis](Point p){return axis?p.y:p.x;};
    auto previous=input.points[input.size-1];
    bool previousInside=lower?coordinate(previous)>=edge:coordinate(previous)<=edge;
    for(int i=0;i<input.size;++i) {
        const auto current=input.points[i];
        const bool inside=lower?coordinate(current)>=edge:coordinate(current)<=edge;
        if(inside!=previousInside) {
            const double t=(edge-coordinate(previous))/(coordinate(current)-coordinate(previous));
            output.points[output.size++]={previous.x+t*(current.x-previous.x),previous.y+t*(current.y-previous.y)};
        }
        if(inside)output.points[output.size++]=current;
        previous=current;previousInside=inside;
    }
    return output;
}
double overlap(const Polygon& drop,int x,int y) {
    auto p=clip(drop,0,x-.5,true);p=clip(p,0,x+.5,false);
    p=clip(p,1,y-.5,true);p=clip(p,1,y+.5,false);
    // Translate near the origin to avoid cancellation for large coordinates.
    double area=0;
    for(int i=0;i<p.size;++i){const auto a=p.points[i],b=p.points[(i+1)%p.size];area+=(a.x-x)*(b.y-y)-(b.x-x)*(a.y-y);}
    return std::abs(area)*.5;
}
double signedArea(const Polygon& p) {
    const auto origin=p.points[0];double area=0;
    for(int i=0;i<p.size;++i){const auto a=p.points[i],b=p.points[(i+1)%p.size];area+=(a.x-origin.x)*(b.y-origin.y)-(b.x-origin.x)*(a.y-origin.y);}
    return area*.5;
}
}
std::array<int,2> drizzleDimensions(int width,int height,const DrizzleOptions& options) {
    if(width<1||height<1||!std::isfinite(options.scale)||options.scale<1||options.scale>2||
       !std::isfinite(options.pixelFraction)||options.pixelFraction<.5||options.pixelFraction>1)
        throw std::runtime_error("Drizzle requires scale 1–2 and pixel fraction 0.5–1.");
    const double w=std::ceil(width*options.scale),h=std::ceil(height*options.scale);
    if(w>astro::domain::maximumImageDimension||h>astro::domain::maximumImageDimension||w*h>astro::domain::maximumImagePixels)throw std::runtime_error("The drizzle output exceeds 128 million pixels. Use a smaller scale or smaller source images.");
    return {int(w),int(h)};
}
namespace {
DrizzleProjection projectPixels(int inputWidth,int inputHeight,const std::vector<float>& samples,
                                domain::BayerPattern pattern,const StarAlignment& alignment,int width,int height,
                                const DrizzleOptions& options,const CheckCancel& check,const WcsProjection* wcs,int firstRow=0,int rowCount=0) {
    const bool cfa=pattern!=domain::BayerPattern::None;
    const int sourceChannels=cfa?1:3,weightChannels=cfa?3:1;
    const auto size=drizzleDimensions(width,height,options);
    if(!astro::domain::supportedImageSize(inputWidth,inputHeight)||samples.size()!=std::size_t(inputWidth)*inputHeight*sourceChannels)
        throw std::runtime_error("Invalid drizzle input dimensions.");
    const auto& m=alignment.matrix;const double determinant=m[0]*m[4]-m[1]*m[3];
    if(std::any_of(m.begin(),m.end(),[](double v){return !std::isfinite(v);})||std::abs(determinant)<.1||std::abs(determinant)>10||
       std::abs(m[0])>4||std::abs(m[1])>4||std::abs(m[3])>4||std::abs(m[4])>4||
       std::abs(m[2])>4.*(width+inputWidth)||std::abs(m[5])>4.*(height+inputHeight))
        throw std::runtime_error("Invalid drizzle registration transform.");
    if(check)check();
    if(!rowCount)rowCount=size[1];
    if(firstRow<0||rowCount<1||firstRow>size[1]-rowCount)throw std::runtime_error("Invalid drizzle strip.");
    const auto pixels=std::size_t(size[0])*rowCount;
    domain::requireImageMemory(std::uint64_t(samples.size())*4+pixels*12*(3+weightChannels)+256ULL*1024*1024);
    std::vector<double> sum(pixels*3),weight(pixels*weightChannels);
    const double scale=options.scale,half=options.pixelFraction*.5;
    const double dropArea=std::abs(determinant)*std::pow(scale*options.pixelFraction,2);
    const bool axisAligned=!wcs&&std::abs(m[1])<1e-12&&std::abs(m[3])<1e-12;
    auto project=[&](double x,double y){if(wcs){const auto p=wcs->forward(x,y);return Point{scale*(p[0]+.5)-.5,scale*(p[1]+.5)-.5};}return Point{scale*(m[0]*x+m[1]*y+m[2]+.5)-.5,scale*(m[3]*x+m[4]*y+m[5]+.5)-.5};};
    std::vector<Polygon> patches;patches.reserve(4);
    // Curved SIP drops are adaptively split. Each leaf stays a convex quad,
    // so clipping has at most eight vertices and fits the fixed buffer.
    auto subdivide=[&](auto&& self,double x0,double y0,double x1,double y1,int depth)->void {
        Polygon p;p.size=4;p.points[0]=project(x0,y0);p.points[1]=project(x1,y0);p.points[2]=project(x1,y1);p.points[3]=project(x0,y1);
        const double mx=(x0+x1)*.5,my=(y0+y1)*.5;
        const std::array<Point,5> actual{project(mx,y0),project(x1,my),project(mx,y1),project(x0,my),project(mx,my)};
        double error=0;for(int n=0;n<5;++n){Point expected{};if(n<4){const auto a=p.points[n],b=p.points[(n+1)%4];expected={(a.x+b.x)*.5,(a.y+b.y)*.5};}else{for(int i=0;i<4;++i){expected.x+=p.points[i].x*.25;expected.y+=p.points[i].y*.25;}}error=std::max(error,std::hypot(actual[n].x-expected.x,actual[n].y-expected.y));}
        if(error>.001){if(depth>=5)throw std::runtime_error("WCS/SIP pixel footprints exceed the supported curvature tolerance.");self(self,x0,y0,mx,my,depth+1);self(self,mx,y0,x1,my,depth+1);self(self,mx,my,x1,y1,depth+1);self(self,x0,my,mx,y1,depth+1);return;}
        const double area=signedArea(p);if(!std::isfinite(area)||std::abs(area)<.1*scale*scale*(x1-x0)*(y1-y0)||std::abs(area)>10*scale*scale*(x1-x0)*(y1-y0))throw std::runtime_error("WCS/SIP projection is folded or outside the supported distortion range.");
        for(int i=0;i<4;++i){const auto a=p.points[i],b=p.points[(i+1)%4],c=p.points[(i+2)%4];if(((b.x-a.x)*(c.y-b.y)-(b.y-a.y)*(c.x-b.x))*area<=0)throw std::runtime_error("WCS/SIP projection is folded or outside the supported distortion range.");}
        patches.push_back(p);
    };
    int sourceTop=0,sourceBottom=inputHeight;
    if(!wcs) {
        // Inverse affine bounds of this strip, expanded by the source drop.
        double low=INFINITY,high=-INFINITY;
        for(double yy:{double(firstRow)-.5,double(firstRow+rowCount)-.5})for(double xx:{-.5,double(size[0])-.5}) {
            const double rx=(xx+.5)/scale-.5-m[2],ry=(yy+.5)/scale-.5-m[5];
            const double sy=(-m[3]*rx+m[0]*ry)/determinant;
            low=std::min(low,sy);high=std::max(high,sy);
        }
        sourceTop=int(std::clamp(std::floor(low-half-1),0.,double(inputHeight)));
        sourceBottom=int(std::clamp(std::ceil(high+half+1),0.,double(inputHeight)));
    }
    // SIP bounds can curve; scan all source rows rather than approximate them.
    for(int y=sourceTop;y<sourceBottom;++y) {
        if(check)check();
        for(int x=0;x<inputWidth;++x) {
            const auto source=(std::size_t(y)*inputWidth+x)*sourceChannels;
            if(!std::isfinite(samples[source])||(!cfa&&(!std::isfinite(samples[source+1])||!std::isfinite(samples[source+2]))))continue;
            const int channel=cfa?domain::bayerChannelAt(pattern,x,y):0;
            patches.clear();
            Polygon drop;drop.size=4;drop.points[0]=project(x-half,y-half);drop.points[1]=project(x+half,y-half);
            drop.points[2]=project(x+half,y+half);drop.points[3]=project(x-half,y+half);
            double pixelArea=dropArea;
            if(wcs){subdivide(subdivide,x-half,y-half,x+half,y+half,0);pixelArea=0;for(const auto& p:patches)pixelArea+=std::abs(signedArea(p));}else patches.push_back(drop);
            for(const auto& drop:patches) {
            double left=drop.points[0].x,right=left,top=drop.points[0].y,bottom=top;
            for(int i=1;i<4;++i){left=std::min(left,drop.points[i].x);right=std::max(right,drop.points[i].x);top=std::min(top,drop.points[i].y);bottom=std::max(bottom,drop.points[i].y);}
            // Bounding coordinates are clipped before integer conversion.
            if(right<=-.5||bottom<=firstRow-.5||left>=size[0]-.5||top>=firstRow+rowCount-.5)continue;
            const int x0=int(std::max(0.,std::floor(left+.5))),x1=int(std::min(double(size[0]-1),std::ceil(right+.5)-1));
            const int y0=int(std::max(double(firstRow),std::floor(top+.5))),y1=int(std::min(double(firstRow+rowCount-1),std::ceil(bottom+.5)-1));
            for(int yy=y0;yy<=y1;++yy)for(int xx=x0;xx<=x1;++xx) {
                const double area=axisAligned?std::max(0.,std::min(right,xx+.5)-std::max(left,xx-.5))*std::max(0.,std::min(bottom,yy+.5)-std::max(top,yy-.5)):overlap(drop,xx,yy);
                if(area<=1e-12*pixelArea)continue;
                const double w=area/pixelArea;const auto target=std::size_t(yy-firstRow)*size[0]+xx;
                if(cfa){weight[target*3+channel]+=w;sum[target*3+channel]+=samples[source]*w;}
                else {weight[target]+=w;for(int c=0;c<3;++c)sum[target*3+c]+=samples[source+c]*w;}
            }
            }
        }
    }
    DrizzleProjection output;output.image={size[0],rowCount,std::vector<float>(pixels*3)};output.weights.resize(weight.size());output.weightChannels=weightChannels;
    for(std::size_t p=0;p<pixels;++p) {
        if(p%65536==0&&check)check();
        for(int c=0;c<3;++c){const auto at=p*weightChannels+(cfa?c:0);output.weights[at]=float(weight[at]);output.image.rgb[p*3+c]=weight[at]>0?float(sum[p*3+c]/weight[at]):std::numeric_limits<float>::quiet_NaN();}
    }
    return output;
}
}
DrizzleProjection drizzleProject(const FloatImage& input,const StarAlignment& alignment,int width,int height,const DrizzleOptions& options,const CheckCancel& check,const WcsProjection* wcs) {
    return projectPixels(input.width,input.height,input.rgb,domain::BayerPattern::None,alignment,width,height,options,check,wcs);
}
DrizzleProjection drizzleProjectBayer(const AstroImage& input,const StarAlignment& alignment,int width,int height,const DrizzleOptions& options,const CheckCancel& check,const WcsProjection* wcs) {
    if(input.channels!=1||input.bayer==domain::BayerPattern::None||domain::bayerChannelAt(input.bayer,0,0)<0)
        throw std::runtime_error("Native Bayer drizzle requires single-channel CFA sources with a recognized Bayer pattern.");
    return projectPixels(input.width,input.height,input.samples,input.bayer,alignment,width,height,options,check,wcs);
}

void drizzleProjectStrips(const FloatImage& input,const StarAlignment& alignment,int width,int height,const DrizzleOptions& options,int rows,const DrizzleStripSink& sink,const CheckCancel& check,const WcsProjection* wcs) {
    const auto size=drizzleDimensions(width,height,options);
    if(rows<1||!sink)throw std::runtime_error("Invalid drizzle strip.");
    for(int y=0;y<size[1];y+=rows){if(check)check();sink(y,projectPixels(input.width,input.height,input.rgb,domain::BayerPattern::None,alignment,width,height,options,check,wcs,y,std::min(rows,size[1]-y)));}
}
void drizzleProjectBayerStrips(const AstroImage& input,const StarAlignment& alignment,int width,int height,const DrizzleOptions& options,int rows,const DrizzleStripSink& sink,const CheckCancel& check,const WcsProjection* wcs) {
    if(input.channels!=1||domain::bayerChannelAt(input.bayer,0,0)<0)throw std::runtime_error("Native Bayer drizzle requires single-channel CFA sources with a recognized Bayer pattern.");
    const auto size=drizzleDimensions(width,height,options);
    if(rows<1||!sink)throw std::runtime_error("Invalid drizzle strip.");
    for(int y=0;y<size[1];y+=rows){if(check)check();sink(y,projectPixels(input.width,input.height,input.samples,input.bayer,alignment,width,height,options,check,wcs,y,std::min(rows,size[1]-y)));}
}
}
