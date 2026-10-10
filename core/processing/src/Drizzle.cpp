#include <astro/processing/DeepSky.hpp>
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
}
std::array<int,2> drizzleDimensions(int width,int height,const DrizzleOptions& options) {
    if(width<1||height<1||!std::isfinite(options.scale)||options.scale<1||options.scale>2||
       !std::isfinite(options.pixelFraction)||options.pixelFraction<.5||options.pixelFraction>1)
        throw std::runtime_error("Drizzle requires scale 1–2 and pixel fraction 0.5–1.");
    const double w=std::ceil(width*options.scale),h=std::ceil(height*options.scale);
    if(w*h>32000000)throw std::runtime_error("The drizzle output exceeds 32 million pixels. Use a smaller scale or smaller source images.");
    return {int(w),int(h)};
}
DrizzleProjection drizzleProject(const FloatImage& input,const StarAlignment& alignment,int width,int height,const DrizzleOptions& options,const CheckCancel& check) {
    const auto size=drizzleDimensions(width,height,options);
    if(input.width<1||input.height<1||std::uint64_t(input.width)*input.height>32000000||input.rgb.size()!=std::size_t(input.width)*input.height*3)
        throw std::runtime_error("Invalid drizzle input dimensions.");
    const auto& m=alignment.matrix;const double determinant=m[0]*m[4]-m[1]*m[3];
    if(std::any_of(m.begin(),m.end(),[](double v){return !std::isfinite(v);})||std::abs(determinant)<.1||std::abs(determinant)>10||
       std::abs(m[0])>4||std::abs(m[1])>4||std::abs(m[3])>4||std::abs(m[4])>4||
       std::abs(m[2])>4.*(width+input.width)||std::abs(m[5])>4.*(height+input.height))
        throw std::runtime_error("Invalid drizzle registration transform.");
    if(check)check();
    const auto pixels=std::size_t(size[0])*size[1];
    std::vector<double> sum(pixels*3),weight(pixels);
    const double scale=options.scale,half=options.pixelFraction*.5;
    const double dropArea=std::abs(determinant)*std::pow(scale*options.pixelFraction,2);
    const bool axisAligned=std::abs(m[1])<1e-12&&std::abs(m[3])<1e-12;
    auto project=[&](double x,double y){return Point{scale*(m[0]*x+m[1]*y+m[2]+.5)-.5,scale*(m[3]*x+m[4]*y+m[5]+.5)-.5};};
    for(int y=0;y<input.height;++y) {
        if(check)check();
        for(int x=0;x<input.width;++x) {
            const auto source=(std::size_t(y)*input.width+x)*3;
            if(!std::isfinite(input.rgb[source])||!std::isfinite(input.rgb[source+1])||!std::isfinite(input.rgb[source+2]))continue;
            Polygon drop;drop.size=4;drop.points[0]=project(x-half,y-half);drop.points[1]=project(x+half,y-half);
            drop.points[2]=project(x+half,y+half);drop.points[3]=project(x-half,y+half);
            double left=drop.points[0].x,right=left,top=drop.points[0].y,bottom=top;
            for(int i=1;i<4;++i){left=std::min(left,drop.points[i].x);right=std::max(right,drop.points[i].x);top=std::min(top,drop.points[i].y);bottom=std::max(bottom,drop.points[i].y);}
            // Bounding coordinates are clipped before integer conversion.
            if(right<=-.5||bottom<=-.5||left>=size[0]-.5||top>=size[1]-.5)continue;
            const int x0=int(std::max(0.,std::floor(left+.5))),x1=int(std::min(double(size[0]-1),std::ceil(right+.5)-1));
            const int y0=int(std::max(0.,std::floor(top+.5))),y1=int(std::min(double(size[1]-1),std::ceil(bottom+.5)-1));
            for(int yy=y0;yy<=y1;++yy)for(int xx=x0;xx<=x1;++xx) {
                const double area=axisAligned?std::max(0.,std::min(right,xx+.5)-std::max(left,xx-.5))*std::max(0.,std::min(bottom,yy+.5)-std::max(top,yy-.5)):overlap(drop,xx,yy);
                if(area<=1e-12*dropArea)continue;
                const double w=area/dropArea;const auto target=std::size_t(yy)*size[0]+xx;
                weight[target]+=w;for(int c=0;c<3;++c)sum[target*3+c]+=input.rgb[source+c]*w;
            }
        }
    }
    DrizzleProjection output;output.image={size[0],size[1],std::vector<float>(pixels*3)};output.weights.resize(pixels);
    for(std::size_t p=0;p<pixels;++p) {
        if(p%65536==0&&check)check();
        output.weights[p]=float(weight[p]);
        for(int c=0;c<3;++c)output.image.rgb[p*3+c]=weight[p]>0?float(sum[p*3+c]/weight[p]):std::numeric_limits<float>::quiet_NaN();
    }
    return output;
}
}
