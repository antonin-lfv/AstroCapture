#include <astro/processing/StarLayers.hpp>
#include <opencv2/core.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::processing {
namespace {
void check(const CheckCancel& c){if(c)c();}
void validate(const FloatImage& a){if(a.width<8||a.height<8||std::uint64_t(a.width)*a.height>32000000||a.rgb.size()!=std::size_t(a.width)*a.height*3)throw std::runtime_error("Invalid star-layer image.");}
double luminance(const float* p){return .2126*p[0]+.7152*p[1]+.0722*p[2];}
double median(std::vector<double> a){const auto mid=a.begin()+a.size()/2;std::nth_element(a.begin(),mid,a.end());return *mid;}
double profile(double r2,double fwhm,const PsfOptions& p){if(p.profile==0)return std::exp(-4*std::log(2.)*r2/(fwhm*fwhm));const double alpha=fwhm/(2*std::sqrt(std::pow(2.,1/p.beta)-1));return std::pow(1+r2/(alpha*alpha),-p.beta);}
void adjust(FloatImage& a,double gain,double saturation,double midtones,const CheckCancel& c){for(std::size_t p=0;p<a.rgb.size();p+=3){if(p%65536==0)check(c);const double l=luminance(&a.rgb[p]);for(int k=0;k<3;++k){double v=(l+saturation*(a.rgb[p+k]-l))*gain;if(midtones!=.5&&v>0&&v<1)v=(midtones-1)*v/((2*midtones-1)*v-midtones);a.rgb[p+k]=float(v);}}}
}
StarLayers separateStars(const FloatImage& source,const StarSeparationOptions& o,const CheckCancel& c){validate(source);makePsf(o.psf);if(o.maximumStars<1||o.maximumStars>5000||!std::isfinite(o.fitTolerance)||o.fitTolerance<.01||o.fitTolerance>.5||!std::isfinite(o.saturation)||o.saturation<=0||o.saturation>100)throw std::runtime_error("Invalid stellar separation parameters.");
 for(std::size_t i=0;i<source.rgb.size();++i){if(i%65536==0)check(c);if(!std::isfinite(source.rgb[i]))throw std::runtime_error("Star separation requires finite pixels.");}
 StarLayers result;result.stars={source.width,source.height,std::vector<float>(source.rgb.size())};const auto candidates=detectStars(source,c,o.maximumStars);result.detected=candidates.size();double totalError=0;
 for(const auto& star:candidates){check(c);double cx=star.x,cy=star.y;const int r=std::min(40,std::max(4,int(std::ceil(o.psf.fwhm*3))));int x=int(std::lround(cx)),y=int(std::lround(cy));if(x<r+2||y<r+2||x+r+2>=source.width||y+r+2>=source.height){++result.rejected;continue;}
  // Skip blended sources and refine the centroid at original resolution.
  bool crowded=false;for(const auto& other:candidates)if(&other!=&star&&std::hypot(other.x-cx,other.y-cy)<2*o.psf.fwhm){crowded=true;break;}if(crowded){++result.rejected;continue;}
  std::vector<double> sky;bool saturated=false;for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){const auto* p=&source.rgb[(std::size_t(y+dy)*source.width+x+dx)*3];if(std::hypot(dx,dy)>r*.8)sky.push_back(luminance(p));if(std::hypot(dx,dy)<o.psf.fwhm)for(int k=0;k<3;++k)saturated|=p[k]>=o.saturation;}
  if(saturated){++result.saturated;++result.rejected;continue;}const double background=median(sky);double flux=0,sx=0,sy=0;const int cr=std::max(2,int(std::ceil(o.psf.fwhm*1.5)));for(int dy=-cr;dy<=cr;++dy)for(int dx=-cr;dx<=cr;++dx){const double signal=std::max(0.,luminance(&source.rgb[(std::size_t(y+dy)*source.width+x+dx)*3])-background);flux+=signal;sx+=signal*dx;sy+=signal*dy;}if(flux<=0){++result.rejected;continue;}cx=x+sx/flux;cy=y+sy/flux;if(std::hypot(cx-star.x,cy-star.y)>o.psf.fwhm*1.5){++result.rejected;continue;}
  double bestError=INFINITY,bestWidth=0;cv::Matx<double,4,3> best;
  for(double factor:{.65,.8,1.,1.25,1.5}){const double width=o.psf.fwhm*factor;cv::Matx44d normal=cv::Matx44d::zeros();cv::Matx<double,4,3> rhs=cv::Matx<double,4,3>::zeros();
   for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){const double u=x+dx-cx,v=y+dy-cy;const cv::Vec4d a{1.,u/r,v/r,profile(u*u+v*v,width,o.psf)};const auto* p=&source.rgb[(std::size_t(y+dy)*source.width+x+dx)*3];for(int i=0;i<4;++i){for(int j=0;j<4;++j)normal(i,j)+=a[i]*a[j];for(int k=0;k<3;++k)rhs(i,k)+=a[i]*p[k];}}
   cv::Matx<double,4,3> fit;if(!cv::solve(normal,rhs,fit,cv::DECOMP_CHOLESKY))continue;const double amplitude=.2126*fit(3,0)+.7152*fit(3,1)+.0722*fit(3,2);if(amplitude<=1e-7||fit(3,0)<0||fit(3,1)<0||fit(3,2)<0)continue;double squared=0;for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){const double u=x+dx-cx,v=y+dy-cy,kernel=profile(u*u+v*v,width,o.psf);const auto* p=&source.rgb[(std::size_t(y+dy)*source.width+x+dx)*3];double model=0;for(int k=0;k<3;++k)model+=(k==0?.2126:k==1?.7152:.0722)*(fit(0,k)+fit(1,k)*u/r+fit(2,k)*v/r+fit(3,k)*kernel);squared+=std::pow(luminance(p)-model,2);}const double error=std::sqrt(squared/((2*r+1)*(2*r+1)))/amplitude;if(error<bestError){bestError=error;bestWidth=width;best=fit;}
  }
  if(bestError>o.fitTolerance||!std::isfinite(bestError)){++result.rejected;continue;}++result.accepted;totalError+=bestError;
  for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){const double u=x+dx-cx,v=y+dy-cy,kernel=profile(u*u+v*v,bestWidth,o.psf);const auto p=(std::size_t(y+dy)*source.width+x+dx)*3;for(int k=0;k<3;++k)result.stars.rgb[p+k]+=float(best(3,k)*kernel);}
 }
 result.fitRms=result.accepted?totalError/result.accepted:0;result.background=source;for(std::size_t i=0;i<source.rgb.size();++i){if(i%65536==0)check(c);result.background.rgb[i]=source.rgb[i]-result.stars.rgb[i];}return result;
}
StarLayers finishStarLayers(const FloatImage& source,const StarSeparationOptions& o,const Finish& common,const StarLayerAdjustments& a,const CheckCancel& c){
 for(double v:{a.starGain,a.starSaturation,a.backgroundGain,a.backgroundSaturation,a.backgroundDenoise,a.starMidtones})if(!std::isfinite(v))throw std::runtime_error("Invalid star-layer adjustments.");if(a.starGain<0||a.starGain>3||a.backgroundGain<0||a.backgroundGain>3||a.starSaturation<0||a.starSaturation>3||a.backgroundSaturation<0||a.backgroundSaturation>3||a.backgroundDenoise<0||a.backgroundDenoise>1||a.starMidtones<.001||a.starMidtones>.999)throw std::runtime_error("Invalid star-layer adjustments.");
 if(common.background)throw std::runtime_error("Correct gradients before separating stars.");auto layers=separateStars(source,o,c);layers.image=finishDeepSky(source,common,c);layers.background=finishDeepSky(layers.background,common,c);for(std::size_t i=0;i<source.rgb.size();++i){if(i%65536==0)check(c);layers.stars.rgb[i]=layers.image.rgb[i]-layers.background.rgb[i];}
 const bool neutral=a.starGain==1&&a.starSaturation==1&&a.starMidtones==.5&&a.backgroundGain==1&&a.backgroundSaturation==1&&a.backgroundDenoise==0;
 adjust(layers.background,a.backgroundGain,a.backgroundSaturation,.5,c);if(a.backgroundDenoise>0){Finish f;f.stretch=false;f.denoise=a.backgroundDenoise;layers.background=finishDeepSky(layers.background,f,c);}adjust(layers.stars,a.starGain,a.starSaturation,a.starMidtones,c);
 if(!neutral)for(std::size_t i=0;i<source.rgb.size();++i){if(i%65536==0)check(c);layers.image.rgb[i]=layers.background.rgb[i]+layers.stars.rgb[i];}return layers;
}
}
