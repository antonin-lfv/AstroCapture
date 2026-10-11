#include <astro/domain/ImageLimits.hpp>
#include <astro/processing/WcsProjection.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace astro::processing {
namespace {
using Wcs=recording::CelestialWcs;
using Vec=std::array<double,3>;
constexpr double rad=3.14159265358979323846/180.;
double dot(Vec a,Vec b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
std::array<Vec,3> basis(const Wcs& w) {
    const double a=w.crval[0]*rad,d=w.crval[1]*rad;
    return {{{-std::sin(a),std::cos(a),0},{-std::sin(d)*std::cos(a),-std::sin(d)*std::sin(a),std::cos(d)},
             {std::cos(d)*std::cos(a),std::cos(d)*std::sin(a),std::sin(d)}}};
}
std::array<double,9> plane(const Wcs& a,const Wcs& b) {
    const auto ab=basis(a),bb=basis(b);const double det=b.cd[0]*b.cd[3]-b.cd[1]*b.cd[2];
    std::array<Vec,3> columns;
    for(int n=0;n<3;++n){columns[0][n]=rad*(a.cd[0]*ab[0][n]+a.cd[2]*ab[1][n]);columns[1][n]=rad*(a.cd[1]*ab[0][n]+a.cd[3]*ab[1][n]);columns[2][n]=ab[2][n];}
    std::array<double,9> h{};
    for(int n=0;n<3;++n){const double x=dot(bb[0],columns[n])/rad,y=dot(bb[1],columns[n])/rad;h[n]=(b.cd[3]*x-b.cd[1]*y)/det;h[3+n]=(-b.cd[2]*x+b.cd[0]*y)/det;h[6+n]=dot(bb[2],columns[n]);}
    return h;
}
// Value and first derivatives, without repeated powers or WCS validation.
std::array<double,3> polynomial(const std::array<double,36>& p,int order,double u,double v) {
    std::array<double,6> up{1},vp{1};for(int i=1;i<=order;++i){up[i]=up[i-1]*u;vp[i]=vp[i-1]*v;}
    std::array<double,3> out{};
    for(int i=0;i<=order;++i)for(int j=0;i+j<=order;++j)if(i+j>=2){const double c=p[i*6+j];out[0]+=c*up[i]*vp[j];if(i)out[1]+=c*i*up[i-1]*vp[j];if(j)out[2]+=c*j*up[i]*vp[j-1];}
    return out;
}
std::array<double,2> map(const Wcs& a,const Wcs& b,const std::array<double,9>& h,double x,double y) {
    double u=x+1-a.crpix[0],v=y+1-a.crpix[1];
    if(a.sipOrder){const auto pa=polynomial(a.sipA,a.sipOrder,u,v),pb=polynomial(a.sipB,a.sipOrder,u,v);const double d=(1+pa[1])*(1+pb[2])-pa[2]*pb[1];if(!std::isfinite(d)||d<.1||d>10)throw std::runtime_error("WCS/SIP projection is folded or outside the supported distortion range.");u+=pa[0];v+=pb[0];}
    const double den=h[6]*u+h[7]*v+h[8];
    if(!std::isfinite(den)||den<=1e-6)throw std::runtime_error("WCS fields cannot be projected on a common TAN plane.");
    const double tu=(h[0]*u+h[1]*v+h[2])/den,tv=(h[3]*u+h[4]*v+h[5])/den;u=tu;v=tv;
    if(b.sipOrder)for(int n=0;n<30;++n) {
        const auto pa=polynomial(b.sipA,b.sipOrder,u,v),pb=polynomial(b.sipB,b.sipOrder,u,v);
        const double ea=u+pa[0]-tu,eb=v+pb[0]-tv,d=(1+pa[1])*(1+pb[2])-pa[2]*pb[1];
        if(!std::isfinite(d)||d<.1||d>10)throw std::runtime_error("WCS/SIP projection is folded or outside the supported distortion range.");
        if(std::hypot(ea,eb)<1e-8)break;
        u-=((1+pb[2])*ea-pa[2]*eb)/d;v-=(-pb[1]*ea+(1+pa[1])*eb)/d;
    }
    if(!std::isfinite(u)||!std::isfinite(v)||(b.sipOrder&&std::hypot(u+polynomial(b.sipA,b.sipOrder,u,v)[0]-tu,v+polynomial(b.sipB,b.sipOrder,u,v)[0]-tv)>1e-6))
        throw std::runtime_error("WCS/SIP inverse projection did not converge.");
    return {u+b.crpix[0]-1,v+b.crpix[1]-1};
}
}
WcsProjection::WcsProjection(const Wcs& source,const Wcs& target,int width,int height,const CheckCancel& check):source_(source),target_(target) {
    if(!source.valid()||!target.valid()||!domain::supportedImageSize(width,height))
        throw std::runtime_error("WCS projection requires valid TAN or TAN-SIP coordinates on every light, including the reference.");
    forward_=plane(source,target);inverse_=plane(target,source);
    double parity=0;
    for(int y=0;y<=16;++y){if(check)check();for(int x=0;x<=16;++x){
        const double sx=-.5+x*width/16.,sy=-.5+y*height/16.;const auto p=forward(sx,sy),back=inverse(p[0],p[1]),dx=forward(sx+.01,sy),dy=forward(sx,sy+.01);
        const double determinant=((dx[0]-p[0])*(dy[1]-p[1])-(dx[1]-p[1])*(dy[0]-p[0]))/1e-4;
        if(std::hypot(back[0]-sx,back[1]-sy)>1e-5||std::abs(determinant)<.1||std::abs(determinant)>10||
           (parity&&determinant*parity<=0)||std::abs(p[0])>8.*width||std::abs(p[1])>8.*height)
            throw std::runtime_error("WCS/SIP projection is folded or outside the supported distortion range.");
        parity=determinant;
    }}
}
std::array<double,2> WcsProjection::forward(double x,double y) const {return map(source_,target_,forward_,x,y);}
std::array<double,2> WcsProjection::inverse(double x,double y) const {return map(target_,source_,inverse_,x,y);}
StarAlignment validateWcsRegistration(const FloatImage& reference,const FloatImage& source,const WcsProjection& projection,const CheckCancel& check) {
    const auto a=detectStars(reference,check,300),b=detectStars(source,check,300);
    struct Match {int r,s;double distance;};std::vector<Match> candidates;
    for(int s=0;s<int(b.size());++s){if(check)check();const auto p=projection.forward(b[s].x,b[s].y);for(int r=0;r<int(a.size());++r){const double d=std::hypot(p[0]-a[r].x,p[1]-a[r].y);if(d<1.5)candidates.push_back({r,s,d});}}
    std::sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.distance<b.distance;});
    std::vector<bool> usedA(a.size()),usedB(b.size());std::array<bool,9> cells{};std::vector<double> ratios;StarAlignment out;double error=0,minX=reference.width,maxX=0,minY=reference.height,maxY=0;
    for(const auto m:candidates)if(!usedA[m.r]&&!usedB[m.s]) {
        usedA[m.r]=usedB[m.s]=true;++out.matches;error+=m.distance*m.distance;
        const auto& p=a[m.r];minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);
        cells[std::clamp(int(p.y*3/reference.height),0,2)*3+std::clamp(int(p.x*3/reference.width),0,2)]=true;
        if(b[m.s].flux>0)ratios.push_back(p.flux/b[m.s].flux);
    }
    out.residual=out.matches?std::sqrt(error/out.matches):0;
    if(out.matches<12||out.residual>.75||std::count(cells.begin(),cells.end(),true)<4||maxX-minX<reference.width*.35||maxY-minY<reference.height*.35)
        throw std::runtime_error("WCS registration failed the star check: at least 12 distributed matches and RMS at most 0.75 pixel are required.");
    if(!ratios.empty()){auto mid=ratios.begin()+ratios.size()/2;std::nth_element(ratios.begin(),mid,ratios.end());out.fluxScale=std::clamp(*mid,.1,10.);}
    return out;
}
FloatImage reprojectWcs(const FloatImage& source,const WcsProjection& projection,int width,int height,const CheckCancel& check) {
    if(!domain::supportedImageSize(width,height)||!domain::supportedImageSize(source.width,source.height)||source.rgb.size()!=std::size_t(source.width)*source.height*3)
        throw std::runtime_error("Invalid WCS projection image dimensions.");
    FloatImage out{width,height,std::vector<float>(std::size_t(width)*height*3,std::numeric_limits<float>::quiet_NaN())};
    for(int y=0;y<height;++y){if(check)check();for(int x=0;x<width;++x){const auto p=projection.inverse(x,y);const double sx=p[0],sy=p[1];if(sx<0||sy<0||sx>source.width-1||sy>source.height-1)continue;
        const int xx=int(sx),yy=int(sy),x1=std::min(xx+1,source.width-1),y1=std::min(yy+1,source.height-1);const double dx=sx-xx,dy=sy-yy;
        const std::array<double,4> weights{(1-dx)*(1-dy),dx*(1-dy),(1-dx)*dy,dx*dy};const std::array<std::size_t,4> indices{std::size_t(yy)*source.width+xx,std::size_t(yy)*source.width+x1,std::size_t(y1)*source.width+xx,std::size_t(y1)*source.width+x1};
        for(int c=0;c<3;++c){double value=0;for(int n=0;n<4;++n)if(weights[n]>1e-12)value+=weights[n]*source.rgb[indices[n]*3+c];out.rgb[(std::size_t(y)*width+x)*3+c]=float(value);}
    }}return out;
}
} // namespace astro::processing
