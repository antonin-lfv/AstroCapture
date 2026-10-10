#pragma once
#include <array>
#include <cmath>
#include <optional>
#include <algorithm>
namespace astro::recording {
// FITS TAN, ICRS. CRPIX is one-based; public pixel coordinates are zero-based
// in the stored row order (the FITS writer never reverses image rows).
struct CelestialWcs {
  std::array<double, 2> crval{}, crpix{};
  std::array<double, 4> cd{};
  // SIP forward polynomials in pixel offsets; index i*6+j is u^i v^j.
  // Inverse coordinates are solved numerically, including after affine remaps.
  int sipOrder{};
  std::array<double,36> sipA{}, sipB{};
  static double polynomial(const std::array<double,36>& p,int order,double u,double v,int du=0,int dv=0) {
    double sum=0;
    for(int i=du;i<=order;++i)for(int j=dv;i+j<=order;++j) {
      if(i+j<2)continue;
      sum+=p[i*6+j]*(du?i:1)*(dv?j:1)*std::pow(u,i-du)*std::pow(v,j-dv);
    }
    return sum;
  }
  bool valid() const {
    if(sipOrder!=0&&(sipOrder<2||sipOrder>5))return false;
    for(int i=0;i<6;++i)for(int j=0;j<6;++j) {
      if(!std::isfinite(sipA[i*6+j])||!std::isfinite(sipB[i*6+j]))return false;
      if((i+j<2||i+j>sipOrder)&&(sipA[i*6+j]!=0||sipB[i*6+j]!=0))return false;
    }
    for (double v : crval)
      if (!std::isfinite(v))
        return false;
    for (double v : crpix)
      if (!std::isfinite(v))
        return false;
    for (double v : cd)
      if (!std::isfinite(v))
        return false;
    return crval[0] >= 0 && crval[0] < 360 && std::abs(crval[1]) <= 90 &&
           std::abs(cd[0] * cd[3] - cd[1] * cd[2]) > 1e-16;
  }
  std::optional<std::array<double, 2>> tangent(double ra, double dec) const {
    constexpr double r = 3.14159265358979323846 / 180.;
    if (!valid() || !std::isfinite(ra) || !std::isfinite(dec) ||
        std::abs(dec) > 90)
      return {};
    const double a = std::remainder(ra - crval[0], 360.) * r, d = dec * r,
                 d0 = crval[1] * r;
    const double den =
        std::sin(d) * std::sin(d0) + std::cos(d) * std::cos(d0) * std::cos(a);
    if (den <= 0)
      return {};
    return std::array<double, 2>{std::cos(d) * std::sin(a) / den / r,
                                 (std::sin(d) * std::cos(d0) -
                                  std::cos(d) * std::sin(d0) * std::cos(a)) /
                                     den / r};
  }
  std::optional<std::array<double, 2>> pixel(double ra, double dec) const {
    const auto p = tangent(ra, dec);
    if (!p)
      return {};
    const double det = cd[0] * cd[3] - cd[1] * cd[2];
    const double targetU=(cd[3]*(*p)[0]-cd[1]*(*p)[1])/det,
                 targetV=(-cd[2]*(*p)[0]+cd[0]*(*p)[1])/det;
    double u=targetU,v=targetV;
    for(int n=0;sipOrder&&n<30;++n) {
      const double a=u+polynomial(sipA,sipOrder,u,v)-targetU,
                   b=v+polynomial(sipB,sipOrder,u,v)-targetV;
      if(std::hypot(a,b)<1e-8)break;
      const double aa=1+polynomial(sipA,sipOrder,u,v,1),ab=polynomial(sipA,sipOrder,u,v,0,1),
                   ba=polynomial(sipB,sipOrder,u,v,1),bb=1+polynomial(sipB,sipOrder,u,v,0,1),d=aa*bb-ab*ba;
      if(!std::isfinite(d)||d<=1e-8)return {};
      u-=(bb*a-ab*b)/d;v-=(-ba*a+aa*b)/d;
    }
    if(!std::isfinite(u)||!std::isfinite(v)||
       std::hypot(u+polynomial(sipA,sipOrder,u,v)-targetU,v+polynomial(sipB,sipOrder,u,v)-targetV)>1e-6)return {};
    return std::array<double,2>{crpix[0]-1+u,crpix[1]-1+v};
  }
  std::optional<std::array<double, 2>> sky(double x, double y) const {
    if (!valid() || !std::isfinite(x) || !std::isfinite(y))
      return {};
    constexpr double r = 3.14159265358979323846 / 180.;
    x += 1 - crpix[0];
    y += 1 - crpix[1];
    const double uPixel=x+polynomial(sipA,sipOrder,x,y),vPixel=y+polynomial(sipB,sipOrder,x,y);
    x=uPixel;y=vPixel;
    const double u = (cd[0] * x + cd[1] * y) * r,
                 v = (cd[2] * x + cd[3] * y) * r, d0 = crval[1] * r;
    const double den = std::cos(d0) - v * std::sin(d0);
    double ra = crval[0] + std::atan2(u, den) / r;
    ra = std::fmod(ra + 720., 360.);
    return std::array<double, 2>{
        ra,
        std::atan2(std::sin(d0) + v * std::cos(d0), std::hypot(den, u)) / r};
  }
  double scale() const {
    return std::sqrt(std::abs(cd[0] * cd[3] - cd[1] * cd[2])) * 3600;
  }
  CelestialWcs remap(const std::array<double, 6> &m) const {
    CelestialWcs out = *this;
    const double determinant = m[0] * m[4] - m[1] * m[3];
    const double x = crpix[0] - 1, y = crpix[1] - 1;
    out.crpix = {1 + m[0] * x + m[1] * y + m[2],
                 1 + m[3] * x + m[4] * y + m[5]};
    out.cd = {(cd[0] * m[4] - cd[1] * m[3]) / determinant,
              (-cd[0] * m[1] + cd[1] * m[0]) / determinant,
              (cd[2] * m[4] - cd[3] * m[3]) / determinant,
              (-cd[2] * m[1] + cd[3] * m[0]) / determinant};
    if(sipOrder) {
      out.sipA.fill(0);out.sipB.fill(0);
      const double a=m[4]/determinant,b=-m[1]/determinant,c=-m[3]/determinant,d=m[0]/determinant;
      auto choose=[](int n,int k){double value=1;for(int t=1;t<=k;++t)value=value*(n-t+1)/t;return value;};
      for(int i=0;i<=sipOrder;++i)for(int j=0;i+j<=sipOrder;++j) {
        if(i+j<2)continue;
        for(int k=0;k<=i;++k)for(int l=0;l<=j;++l) {
          const double factor=choose(i,k)*choose(j,l)*std::pow(a,k)*std::pow(b,i-k)*std::pow(c,l)*std::pow(d,j-l);
          const int at=(k+l)*6+i+j-k-l;
          out.sipA[at]+=factor*(m[0]*sipA[i*6+j]+m[1]*sipB[i*6+j]);
          out.sipB[at]+=factor*(m[3]*sipA[i*6+j]+m[4]*sipB[i*6+j]);
        }
      }
    }
    return out;
  }
};
} // namespace astro::recording
