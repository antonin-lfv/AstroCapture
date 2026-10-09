#include <algorithm>
#include <astro/processing/DeepSkyAdvanced.hpp>
#include <cmath>
#include <numeric>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>

namespace astro::processing {
namespace {
void checkpoint(const CheckCancel &c) {
  if (c)
    c();
}
void validate(const FloatImage &image) {
  if (image.width < 1 || image.height < 1 ||
      std::uint64_t(image.width) * image.height > 32000000 ||
      image.rgb.size() != std::size_t(image.width) * image.height * 3)
    throw std::runtime_error("Invalid advanced processing image.");
}
double median(std::vector<double> v) {
  if (v.empty())
    throw std::runtime_error("No valid samples.");
  const auto m = v.begin() + v.size() / 2;
  std::nth_element(v.begin(), m, v.end());
  return *m;
}
double mad(const std::vector<double> &v) {
  const double m = median(v);
  std::vector<double> d;
  d.reserve(v.size());
  for (double x : v)
    d.push_back(std::abs(x - m));
  return median(std::move(d));
}
float luminance(const float *p) {
  return .2126f * p[0] + .7152f * p[1] + .0722f * p[2];
}
double noise(const cv::Mat &plane) {
  std::vector<double> d;
  const auto step =
      std::max(1, int(std::sqrt(double(plane.total()) / 100000.)));
  for (int y = 0; y < plane.rows; y += step)
    for (int x = 0; x + 1 < plane.cols; x += step) {
      const double v = plane.at<float>(y, x + 1) - plane.at<float>(y, x);
      if (std::isfinite(v))
        d.push_back(v);
    }
  return d.empty() ? 0 : 1.4826 * mad(d) / std::sqrt(2.);
}
int reflect(int x, int size) {
  if (size == 1)
    return 0;
  while (x < 0 || x >= size)
    x = x < 0 ? -x : 2 * size - 2 - x;
  return x;
}
cv::Mat atrous(const cv::Mat &source, int step, const CheckCancel &check) {
  cv::Mat tmp(source.size(), CV_32F), out(source.size(), CV_32F);
  constexpr float k[5] = {1.f / 16, 4.f / 16, 6.f / 16, 4.f / 16, 1.f / 16};
  for (int y = 0; y < source.rows; ++y) {
    checkpoint(check);
    for (int x = 0; x < source.cols; ++x) {
      float v = 0;
      for (int t = -2; t <= 2; ++t)
        v += k[t + 2] * source.at<float>(y, reflect(x + t * step, source.cols));
      tmp.at<float>(y, x) = v;
    }
  }
  for (int y = 0; y < source.rows; ++y) {
    checkpoint(check);
    for (int x = 0; x < source.cols; ++x) {
      float v = 0;
      for (int t = -2; t <= 2; ++t)
        v += k[t + 2] * tmp.at<float>(reflect(y + t * step, source.rows), x);
      out.at<float>(y, x) = v;
    }
  }
  return out;
}
} // namespace
GhsCurve::GhsCurve(const GhsOptions &o) : options_(o) {
  for (double v :
       {o.strength, o.localIntensity, o.symmetry, o.shadows, o.highlights})
    if (!std::isfinite(v))
      throw std::runtime_error("Invalid GHS parameters.");
  if (o.strength < 0 || o.strength > 20 || o.localIntensity < -5 ||
      o.localIntensity > 15 || o.shadows < 0 || o.shadows > o.symmetry ||
      o.symmetry > o.highlights || o.highlights > 1 || o.mode < 0 || o.mode > 1)
    throw std::runtime_error(
        "GHS requires 0 ≤ shadows ≤ symmetry ≤ highlights ≤ 1.");
  d_ = std::expm1(o.strength);
  if (d_ > 0) {
    low_ = raw(0);
    range_ = raw(1) - low_;
    if (!(range_ > 0) || !std::isfinite(range_))
      throw std::runtime_error("Degenerate GHS curve.");
  }
}
double GhsCurve::base(double x) const {
  const double b = options_.localIntensity;
  if (d_ == 0)
    return x;
  if (b == -1)
    return std::log1p(d_ * x);
  if (b < 0)
    return -std::expm1(((b + 1) / b) * std::log1p(-b * d_ * x)) /
           (d_ * (b + 1));
  if (b == 0)
    return -std::expm1(-d_ * x);
  return -std::expm1(-std::log1p(b * d_ * x) / b);
}
double GhsCurve::derivative(double x) const {
  const double b = options_.localIntensity;
  if (d_ == 0)
    return 1;
  if (b == -1)
    return d_ / (1 + d_ * x);
  if (b < 0)
    return std::exp(std::log1p(-b * d_ * x) / b);
  if (b == 0)
    return d_ * std::exp(-d_ * x);
  return d_ * std::exp(-(1 + b) / b * std::log1p(b * d_ * x));
}
double GhsCurve::raw(double x) const {
  const auto &o = options_;
  if (x < o.shadows)
    return -base(o.symmetry - o.shadows) +
           derivative(o.symmetry - o.shadows) * (x - o.shadows);
  if (x < o.symmetry)
    return -base(o.symmetry - x);
  if (x <= o.highlights)
    return base(x - o.symmetry);
  return base(o.highlights - o.symmetry) +
         derivative(o.highlights - o.symmetry) * (x - o.highlights);
}
double GhsCurve::operator()(double x) const {
  x = std::clamp(x, 0., 1.);
  if (d_ == 0)
    return x;
  if (!options_.inverse)
    return std::clamp((raw(x) - low_) / range_, 0., 1.);
  double a = 0, b = 1;
  for (int n = 0; n < 52; ++n) {
    const double m = (a + b) * .5;
    if ((raw(m) - low_) / range_ < x)
      a = m;
    else
      b = m;
  }
  return (a + b) * .5;
}
std::vector<float> makeMask(const FloatImage &image, const MaskOptions &o,
                            const CheckCancel &check) {
  validate(image);
  for (double v :
       {o.low, o.high, o.feather, o.centerX, o.centerY, o.radius, o.starRadius})
    if (!std::isfinite(v))
      throw std::runtime_error("Invalid mask parameters.");
  if (o.type < 0 || o.type > 5 || o.low < 0 || o.high > 1 || o.low >= o.high ||
      o.feather <= 0 || o.feather > 1 || o.radius <= 0 || o.radius > 2 ||
      o.centerX < 0 || o.centerX > 1 || o.centerY < 0 || o.centerY > 1 ||
      o.starRadius < .5 || o.starRadius > 6)
    throw std::runtime_error("Invalid mask ranges.");
  std::vector<float> mask(std::size_t(image.width) * image.height,
                          o.type == 3 || ((o.type==4||o.type==5)&&!o.baseWhite) ? 0.f : 1.f);
  const double scale = std::min(image.width, image.height);
  auto smooth = [](double x) {
    x = std::clamp(x, 0., 1.);
    return x * x * (3 - 2 * x);
  };
  if (!std::isfinite(o.blurRadius)||o.blurRadius<0||o.blurRadius>100||o.strokes.size()>200)
    throw std::runtime_error("Invalid local mask parameters.");
  if(o.type==4||o.type==5) {
    if(o.type==5){if(o.raster.size()!=mask.size())throw std::runtime_error("The imported mask must match the image dimensions.");for(float v:o.raster)if(!std::isfinite(v)||v<0||v>1)throw std::runtime_error("Mask values must be between zero and one.");mask=o.raster;}
    std::size_t pointCount=0;
    for(const auto& stroke:o.strokes){
      checkpoint(check);pointCount+=stroke.points.size();
      if(stroke.points.empty()||pointCount>10000||!std::isfinite(stroke.radius)||stroke.radius<=0||stroke.radius>.5||!std::isfinite(stroke.hardness)||stroke.hardness<0||stroke.hardness>1||!std::isfinite(stroke.opacity)||stroke.opacity<0||stroke.opacity>1||(stroke.value!=0&&stroke.value!=1))throw std::runtime_error("Invalid mask brush stroke.");
      double minX=image.width,minY=image.height,maxX=0,maxY=0;const double radius=stroke.radius*scale;
      for(const auto& p:stroke.points){if(!std::isfinite(p[0])||!std::isfinite(p[1])||p[0]<0||p[0]>1||p[1]<0||p[1]>1)throw std::runtime_error("Invalid mask brush coordinates.");minX=std::min(minX,p[0]*image.width);maxX=std::max(maxX,p[0]*image.width);minY=std::min(minY,p[1]*image.height);maxY=std::max(maxY,p[1]*image.height);}
      const int left=std::max(0,int(std::floor(minX-radius))),top=std::max(0,int(std::floor(minY-radius))),right=std::min(image.width-1,int(std::ceil(maxX+radius))),bottom=std::min(image.height-1,int(std::ceil(maxY+radius))),width=right-left+1;
      std::vector<float> coverage(std::size_t(width)*(bottom-top+1),0);
      for(std::size_t n=0;n<stroke.points.size();++n){const auto a=stroke.points[n? n-1:0],b=stroke.points[n];const double ax=a[0]*image.width,ay=a[1]*image.height,bx=b[0]*image.width,by=b[1]*image.height,dx=bx-ax,dy=by-ay,length=dx*dx+dy*dy;
        for(int y=std::max(top,int(std::floor(std::min(ay,by)-radius)));y<=std::min(bottom,int(std::ceil(std::max(ay,by)+radius)));++y){checkpoint(check);for(int x=std::max(left,int(std::floor(std::min(ax,bx)-radius)));x<=std::min(right,int(std::ceil(std::max(ax,bx)+radius)));++x){const double t=length?std::clamp(((x+.5-ax)*dx+(y+.5-ay)*dy)/length,0.,1.):0;const double r=std::hypot(x+.5-ax-t*dx,y+.5-ay-t*dy)/radius;const float weight=float(r>=1?0:r<=stroke.hardness?1:1-smooth((r-stroke.hardness)/(1-stroke.hardness)));auto& v=coverage[std::size_t(y-top)*width+x-left];v=std::max(v,weight);}}}
      // Opacity is applied once per continuous stroke, including overlapping segments.
      for(int y=top;y<=bottom;++y){checkpoint(check);for(int x=left;x<=right;++x){const float amount=coverage[std::size_t(y-top)*width+x-left]*stroke.opacity;auto& v=mask[std::size_t(y)*image.width+x];v=float(v+amount*(stroke.value-v));}}
    }
  } else if (o.type == 3) {
    const auto stars = detectStars(image, check);
    if (stars.empty())
      throw std::runtime_error("No stars found for the mask.");
    for (const auto &s : stars) {
      checkpoint(check);
      const double radius = std::max(1., s.fwhm * o.starRadius);
      const int extent = int(std::ceil(radius * (1 + o.feather)));
      for (int y = std::max(0, int(s.y) - extent);
           y <= std::min(image.height - 1, int(s.y) + extent); ++y)
        for (int x = std::max(0, int(s.x) - extent);
             x <= std::min(image.width - 1, int(s.x) + extent); ++x) {
          const double r = std::hypot(x - s.x, y - s.y);
          const float v =
              float(1 - smooth((r - radius) / (radius * o.feather)));
          auto &m = mask[std::size_t(y) * image.width + x];
          m = std::max(m, v);
        }
    }
  } else if (o.type != 0) {
    for (int y = 0; y < image.height; ++y) {
      checkpoint(check);
      for (int x = 0; x < image.width; ++x) {
        double v;
        if (o.type == 1) {
          const double l =
              luminance(&image.rgb[(std::size_t(y) * image.width + x) * 3]);
          v = smooth((l - o.low) / o.feather) *
              smooth((o.high - l) / o.feather);
        } else {
          const double r = std::hypot((x + .5) - o.centerX * image.width,
                                      (y + .5) - o.centerY * image.height) /
                           scale;
          v = 1 - smooth((r - o.radius) / o.feather);
        }
        mask[std::size_t(y) * image.width + x] = float(v);
      }
    }
  }
  if(o.blurRadius>0&&o.type!=0){checkpoint(check);cv::Mat plane(image.height,image.width,CV_32F,mask.data());cv::GaussianBlur(plane,plane,cv::Size(),o.blurRadius,o.blurRadius,cv::BORDER_REPLICATE);checkpoint(check);for(auto& v:mask)v=std::clamp(v,0.f,1.f);}
  if (o.invert && o.type != 0)
    for (auto &v : mask)
      v = 1 - v;
  return mask;
}
void blendMask(const FloatImage &original, FloatImage &result,
               const std::vector<float> &mask, const CheckCancel &check) {
  validate(original);
  validate(result);
  if (original.width != result.width || original.height != result.height ||
      mask.size() != original.rgb.size() / 3)
    throw std::runtime_error("Mask dimensions do not match.");
  for (std::size_t p = 0; p < mask.size(); ++p) {
    if (p % 65536 == 0)
      checkpoint(check);
    if (!std::isfinite(mask[p]) || mask[p] < 0 || mask[p] > 1)
      throw std::runtime_error("Invalid mask values.");
    for (int c = 0; c < 3; ++c) {
      const auto i = p * 3 + c;
      result.rgb[i] =
          mask[p] == 0 ? original.rgb[i]
          : mask[p] == 1
              ? result.rgb[i]
              : original.rgb[i] + mask[p] * (result.rgb[i] - original.rgb[i]);
    }
  }
}
PsfEstimate estimatePsf(const FloatImage &image, const CheckCancel &check) {
  validate(image);
  std::vector<double> widths;
  for (const auto &s : detectStars(image, check, 1500)) {
    checkpoint(check);
    if (s.x < image.width * .15 || s.x > image.width * .85 ||
        s.y < image.height * .15 || s.y > image.height * .85)
      continue;
    const int x = int(std::lround(s.x)), y = int(std::lround(s.y)), r = 10;
    if (x < r || y < r || x + r >= image.width || y + r >= image.height)
      continue;
    std::vector<double> sky;
    double peak = 0;
    for (int dy = -r; dy <= r; ++dy)
      for (int dx = -r; dx <= r; ++dx) {
        const auto *p =
            &image.rgb[(std::size_t(y + dy) * image.width + x + dx) * 3];
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) ||
            !std::isfinite(p[2]))
          continue;
        peak = std::max(peak, double(*std::max_element(p, p + 3)));
        if (std::hypot(dx, dy) > 8)
          sky.push_back(luminance(p));
      }
    if (sky.size() < 30 || peak >= .98)
      continue;
    const double bg = median(sky);
    double sum = 0, sx = 0, sy = 0, sxx = 0, syy = 0;
    for (int dy = -7; dy <= 7; ++dy)
      for (int dx = -7; dx <= 7; ++dx) {
        const double v = std::max(
            0.,
            double(luminance(
                &image.rgb[(std::size_t(y + dy) * image.width + x + dx) * 3])) -
                bg);
        sum += v;
        sx += v * dx;
        sy += v * dy;
        sxx += v * dx * dx;
        syy += v * dy * dy;
      }
    if (sum <= 0)
      continue;
    const double vx = sxx / sum - std::pow(sx / sum, 2),
                 vy = syy / sum - std::pow(sy / sum, 2);
    if (vx <= 0 || vy <= 0 || std::max(vx, vy) > 2 * std::min(vx, vy))
      continue;
    const double fwhm = 2.354820045 * std::sqrt((vx + vy) / 2);
    if (fwhm >= .5 && fwhm <= 12)
      widths.push_back(fwhm);
  }
  if (widths.size() < 3)
    throw std::runtime_error("At least three unsaturated central stars are "
                             "required to estimate a circular Gaussian PSF.");
  return {median(widths), 1.4826 * mad(widths), int(widths.size())};
}
std::vector<float> makePsf(const PsfOptions &o, int *size) {
  if (o.profile < 0 || o.profile > 1 || !std::isfinite(o.fwhm) || o.fwhm < .5 ||
      o.fwhm > 12 || !std::isfinite(o.beta) || o.beta < 1.5 || o.beta > 10)
    throw std::runtime_error("Invalid PSF parameters.");
  const int r = std::min(
                40,
                std::max(2, int(std::ceil(o.fwhm * (o.profile == 0 ? 2 : 3))))),
            n = 2 * r + 1;
  std::vector<float> kernel(n * n);
  const double sigma = o.fwhm / 2.354820045,
               alpha =
                   o.fwhm / (2 * std::sqrt(std::expm1(std::log(2.) / o.beta)));
  double sum = 0;
  for (int y = -r; y <= r; ++y)
    for (int x = -r; x <= r; ++x) {
      const double d = x * x + y * y;
      const float v =
          float(o.profile == 0 ? std::exp(-d / (2 * sigma * sigma))
                               : std::pow(1 + d / (alpha * alpha), -o.beta));
      kernel[(y + r) * n + x + r] = v;
      sum += v;
    }
  for (auto &v : kernel)
    v = float(v / sum);
  if (size)
    *size = n;
  return kernel;
}
DeconvolutionResult deconvolve(const FloatImage &input,
                               const DeconvolutionOptions &o,
                               const CheckCancel &check,
                               const Progress &progress) {
  validate(input);
  if (o.iterations < 1 || o.iterations > 100 || !std::isfinite(o.damping) ||
      o.damping < 0 || o.damping > 1 || !std::isfinite(o.blend) ||
      o.blend < 0 || o.blend > 1 || !std::isfinite(o.stopping) ||
      o.stopping < 0 || o.stopping > .1)
    throw std::runtime_error("Invalid deconvolution settings.");
  int n;
  auto kernel = makePsf(o.psf, &n);
  cv::Mat psf(n, n, CV_32F, kernel.data());
  DeconvolutionResult result{input};
  if (o.blend == 0)
    return result;
  cv::Mat observed(input.height, input.width, CV_32F);
  for (int y = 0; y < input.height; ++y) {
    checkpoint(check);
    for (int x = 0; x < input.width; ++x) {
      const auto *p = &input.rgb[(std::size_t(y) * input.width + x) * 3];
      for (int c = 0; c < 3; ++c)
        if (!std::isfinite(p[c]) || p[c] < 0)
          throw std::runtime_error(
              "Richardson-Lucy requires finite non-negative input. Adjust the "
              "pedestal before deconvolution.");
      observed.at<float>(y, x) = luminance(p);
    }
  }
  cv::Mat estimate = observed.clone(), blur, ratio(observed.size(), CV_32F),
          correction;
  for (int it = 0; it < o.iterations; ++it) {
    checkpoint(check);
    cv::filter2D(estimate, blur, -1, psf, cv::Point(-1, -1), 0,
                 cv::BORDER_REFLECT_101);
    for (int y = 0; y < input.height; ++y) {
      checkpoint(check);
      for (int x = 0; x < input.width; ++x) {
        const double obs = observed.at<float>(y, x), b = blur.at<float>(y, x),
                     residual = obs - b;
        const double w =
            o.damping == 0 ? 1
                           : residual * residual /
                                 (residual * residual + o.damping * o.damping);
        ratio.at<float>(y, x) = float(1 + w * (obs / std::max(b, 1e-12) - 1));
      }
    }
    cv::filter2D(ratio, correction, -1, psf, cv::Point(-1, -1), 0,
                 cv::BORDER_REFLECT_101);
    double delta = 0, total = 0;
    for (int y = 0; y < input.height; ++y) {
      checkpoint(check);
      for (int x = 0; x < input.width; ++x) {
        auto &v = estimate.at<float>(y, x);
        const float next = std::max(0.f, v * correction.at<float>(y, x));
        if (!std::isfinite(next))
          throw std::runtime_error(
              "Deconvolution diverged; reduce iterations or increase damping.");
        delta += std::abs(next - v);
        total += std::abs(v);
        v = next;
      }
    }
    result.iterations = it + 1;
    result.relativeChange = delta / std::max(total, 1e-12);
    if (progress)
      progress(it + 1, o.iterations, "Richardson-Lucy");
    if (o.stopping > 0 && result.relativeChange < o.stopping)
      break;
  }
  for (int y = 0; y < input.height; ++y) {
    checkpoint(check);
    for (int x = 0; x < input.width; ++x) {
      const double l = observed.at<float>(y, x);
      const double f =
          l > 1e-12 ? 1 + o.blend * (estimate.at<float>(y, x) / l - 1) : 1;
      for (int c = 0; c < 3; ++c)
        result.image.rgb[(std::size_t(y) * input.width + x) * 3 + c] *=
            float(f);
    }
  }
  return result;
}
DenoiseResult denoiseMultiscale(const FloatImage &image,
                                const DenoiseOptions &o,
                                const CheckCancel &check) {
  validate(image);
  if (o.scales < 1 || o.scales > 5 || !std::isfinite(o.threshold) ||
      o.threshold < 0 || o.threshold > 8 || !std::isfinite(o.amount) ||
      o.amount < 0 || o.amount > 1 || !std::isfinite(o.chroma) ||
      o.chroma < 0 || o.chroma > 1)
    throw std::runtime_error("Invalid multiscale denoising settings.");
  for (float v : image.rgb)
    if (!std::isfinite(v))
      throw std::runtime_error("Denoising requires finite pixels.");
  DenoiseResult result{image};
  cv::Mat l(image.height, image.width, CV_32F), r(l.size(), CV_32F),
      b(l.size(), CV_32F);
  for (int y = 0; y < image.height; ++y) {
    checkpoint(check);
    for (int x = 0; x < image.width; ++x) {
      const auto *p = &image.rgb[(std::size_t(y) * image.width + x) * 3];
      const float v = luminance(p);
      l.at<float>(y, x) = v;
      r.at<float>(y, x) = p[0] - v;
      b.at<float>(y, x) = p[2] - v;
    }
  }
  result.noiseBefore = noise(l);
  if ((o.amount == 0 && o.chroma == 0) || o.threshold == 0) {
    result.noiseAfter = result.noiseBefore;
    return result;
  }
  auto process = [&](cv::Mat plane, double amount) {
    if (amount == 0 || o.threshold == 0)
      return plane;
    cv::Mat reconstructed = cv::Mat::zeros(plane.size(), CV_32F);
    for (int scale = 0; scale < o.scales; ++scale) {
      auto smooth = atrous(plane, 1 << scale, check);
      std::vector<double> samples;
      const auto stride =
          std::max(1, int(std::sqrt(double(plane.total()) / 100000.)));
      for (int y = 0; y < plane.rows; y += stride)
        for (int x = 0; x < plane.cols; x += stride)
          samples.push_back(plane.at<float>(y, x) - smooth.at<float>(y, x));
      const double threshold = o.threshold * 1.4826 * mad(samples);
      for (int y = 0; y < plane.rows; ++y) {
        checkpoint(check);
        for (int x = 0; x < plane.cols; ++x) {
          const float d = plane.at<float>(y, x) - smooth.at<float>(y, x);
          const float shrink = std::copysign(
              float(std::max(0., std::abs(double(d)) - threshold)), d);
          reconstructed.at<float>(y, x) +=
              float((1 - amount) * d + amount * shrink);
        }
      }
      plane = std::move(smooth);
    }
    return cv::Mat(reconstructed + plane);
  };
  l = process(l, o.amount);
  r = process(r, o.chroma);
  b = process(b, o.chroma);
  result.noiseAfter = noise(l);
  for (int y = 0; y < image.height; ++y) {
    checkpoint(check);
    for (int x = 0; x < image.width; ++x) {
      auto *p = &result.image.rgb[(std::size_t(y) * image.width + x) * 3];
      const float v = l.at<float>(y, x), dr = r.at<float>(y, x),
                  db = b.at<float>(y, x);
      p[0] = v + dr;
      p[2] = v + db;
      p[1] = v - (.2126f * dr + .0722f * db) / .7152f;
    }
  }
  return result;
}
PhotometryResult
calibratePhotometry(const FloatImage &image,
                    const std::vector<PhotometricReference> &references,
                    const PhotometryOptions &o, const CheckCancel &check) {
  validate(image);
  if (references.size() < 3 || references.size() > 500 ||
      !std::isfinite(o.aperture) || !std::isfinite(o.inner) ||
      !std::isfinite(o.outer) || !std::isfinite(o.saturation) ||
      o.aperture < 1 || o.aperture > 30 || o.inner <= o.aperture ||
      o.outer <= o.inner || o.outer > 80 || o.saturation <= 0)
    throw std::runtime_error("Photometry requires at least three references "
                             "and aperture < inner annulus < outer annulus.");
  struct Measurement {
    double r, b;
  };
  std::vector<Measurement> measured;
  PhotometryResult result{image};
  bool color = false;
  for (std::size_t i = 0; i < image.rgb.size(); ++i) {
    if (i % 65536 == 0)
      checkpoint(check);
    const float v = image.rgb[i];
    if (!std::isfinite(v))
      throw std::runtime_error("Photometry requires finite pixels.");
    if (i % 3 != 0 && std::abs(v - image.rgb[i - i % 3]) > 1e-8f)
      color = true;
  }
  if (!color)
    throw std::runtime_error("Relative color calibration requires RGB color "
                             "data, not a duplicated monochrome channel.");
  for (const auto &star : references) {
    checkpoint(check);
    bool valid = std::isfinite(star.x) && std::isfinite(star.y);
    for (double f : star.flux)
      valid &= std::isfinite(f) && f > 0;
    if (!valid)
      throw std::runtime_error("Invalid photometric reference.");
    if (star.x - o.outer < 0 || star.y - o.outer < 0 ||
        star.x + o.outer >= image.width || star.y + o.outer >= image.height) {
      ++result.rejected;
      continue;
    }
    std::array<std::vector<double>, 3> sky;
    std::array<double, 3> sum{};
    int pixels = 0;
    bool saturated = false;
    for (int y = int(std::floor(star.y - o.outer));
         y <= int(std::ceil(star.y + o.outer)); ++y)
      for (int x = int(std::floor(star.x - o.outer));
           x <= int(std::ceil(star.x + o.outer)); ++x) {
        const double radius = std::hypot(x - star.x, y - star.y);
        if (radius > o.outer)
          continue;
        const auto *p = &image.rgb[(std::size_t(y) * image.width + x) * 3];
        for (int c = 0; c < 3; ++c) {
          if (!std::isfinite(p[c])) {
            saturated = true;
            continue;
          }
          if (radius <= o.aperture) {
            sum[c] += p[c];
            saturated |= p[c] >= o.saturation;
          } else if (radius >= o.inner)
            sky[c].push_back(p[c]);
        }
        if (radius <= o.aperture)
          ++pixels;
      }
    if (saturated || pixels < 3 || sky[0].size() < 10 || sky[1].size() < 10 ||
        sky[2].size() < 10) {
      ++result.rejected;
      continue;
    }
    for (int c = 0; c < 3; ++c)
      sum[c] -= pixels * median(sky[c]);
    if (*std::min_element(sum.begin(), sum.end()) <= 0) {
      ++result.rejected;
      continue;
    }
    measured.push_back(
        {std::log((star.flux[0] / star.flux[1]) / (sum[0] / sum[1])),
         std::log((star.flux[2] / star.flux[1]) / (sum[2] / sum[1]))});
  }
  if (measured.size() < 3)
    throw std::runtime_error(
        "Fewer than three usable unsaturated reference stars.");
  std::vector<double> red, blue;
  for (auto m : measured) {
    red.push_back(m.r);
    blue.push_back(m.b);
  }
  const double mr = median(red), mb = median(blue),
               sr = std::max(.01, 1.4826 * mad(red)),
               sb = std::max(.01, 1.4826 * mad(blue));
  red.clear();
  blue.clear();
  for (auto m : measured) {
    if (std::abs(m.r - mr) > 3 * sr || std::abs(m.b - mb) > 3 * sb) {
      ++result.rejected;
      continue;
    }
    red.push_back(m.r);
    blue.push_back(m.b);
  }
  if (red.size() < 3)
    throw std::runtime_error("Photometric references disagree; fewer than "
                             "three remain after rejection.");
  result.accepted = int(red.size());
  result.gains = {std::exp(median(red)), 1, std::exp(median(blue))};
  result.scatter =
      std::max(1.4826 * mad(red), 1.4826 * mad(blue)) * 2.5 / std::log(10.);
  for (double g : result.gains)
    if (!std::isfinite(g) || g < .05 || g > 20)
      throw std::runtime_error("Photometric gains are implausible; verify "
                               "reference fluxes and coordinates.");
  for (std::size_t i = 0; i < image.rgb.size(); ++i) {
    if (i % 65536 == 0)
      checkpoint(check);
    result.image.rgb[i] *= float(result.gains[i % 3]);
  }
  return result;
}
} // namespace astro::processing
