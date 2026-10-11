#include <astro/domain/ImageLimits.hpp>
#include "MetalProcessing.hpp"
#include <algorithm>
#include <astro/processing/Planetary.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace astro::processing {
namespace {
void checkpoint(const CheckCancel &check) {
  if (check)
    check();
}
void validate(const FloatImage &image, std::uint64_t maxPixels = 16000000) {
  if (!domain::supportedImageSize(image.width,image.height) ||
      std::uint64_t(image.width) * image.height > maxPixels ||
      image.rgb.size() != std::size_t(image.width) * image.height * 3)
    throw std::runtime_error("Invalid processing image dimensions.");
}
void validateGray(const GrayImage &image) {
  if (!domain::supportedImageSize(image.width,image.height) ||
      std::uint64_t(image.width) * image.height > 16000000 ||
      image.pixels.size() != std::size_t(image.width) * image.height)
    throw std::runtime_error("Invalid analysis image.");
}
GrayImage half(const GrayImage &image) {
  GrayImage out{image.width / 2, image.height / 2, {}};
  out.pixels.resize(std::size_t(out.width) * out.height);
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x) {
      const auto p = std::size_t(2 * y) * image.width + 2 * x;
      out.pixels[std::size_t(y) * out.width + x] =
          (image.pixels[p] + image.pixels[p + 1] +
           image.pixels[p + image.width] + image.pixels[p + image.width + 1]) *
          0.25f;
    }
  return out;
}
double correlation(const GrayImage &a, const GrayImage &b, int dx, int dy) {
  const int border = std::max(2, std::min(a.width, a.height) / 8);
  const int left = std::max(border, -dx), top = std::max(border, -dy);
  const int right = std::min(a.width - border, b.width - dx),
            bottom = std::min(a.height - border, b.height - dy);
  double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
  const int n = (right - left) * (bottom - top);
  if (right <= left || bottom <= top || n < 16)
    return -1;
  for (int y = top; y < bottom; ++y)
    for (int x = left; x < right; ++x) {
      const double av = a.pixels[std::size_t(y) * a.width + x],
                   bv = b.pixels[std::size_t(y + dy) * b.width + x + dx];
      sa += av;
      sb += bv;
      saa += av * av;
      sbb += bv * bv;
      sab += av * bv;
    }
  const double denom = (saa - sa * sa / n) * (sbb - sb * sb / n);
  return denom <= 1e-16 ? -1 : (sab - sa * sb / n) / std::sqrt(denom);
}
} // namespace
FloatImage decodeSamples(std::span<const std::uint16_t> samples, int width,
                         int height, int channels, int bits,
                         domain::BayerPattern bayer) {
  if (width <= 0 || height <= 0 || std::uint64_t(width) * height > 16000000 ||
      (channels != 1 && channels != 3) || bits < 1 || bits > 16 ||
      samples.size() != std::size_t(width) * height * channels ||
      (bayer != domain::BayerPattern::None &&
       (channels != 1 || width < 2 || height < 2)))
    throw std::runtime_error("Unsupported sample layout.");
  FloatImage out{width, height,
                 std::vector<float>(std::size_t(width) * height * 3)};
  const float scale = 1.f / float((1u << bits) - 1);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const auto p = std::size_t(y) * width + x;
      for (int c = 0; c < 3; ++c) {
        float value = 0;
        if (channels == 3)
          value = samples[p * 3 + c];
        else if (bayer == domain::BayerPattern::None ||
                 domain::bayerChannelAt(bayer, x, y) == c)
          value = samples[p];
        else {
          int n = 0;
          for (int yy = std::max(0, y - 1); yy <= std::min(height - 1, y + 1);
               ++yy)
            for (int xx = std::max(0, x - 1); xx <= std::min(width - 1, x + 1);
                 ++xx)
              if (domain::bayerChannelAt(bayer, xx, yy) == c) {
                value += samples[std::size_t(yy) * width + xx];
                ++n;
              }
          if (n)
            value /= float(n);
        }
        out.rgb[p * 3 + c] = value * scale;
      }
    }
  return out;
}
GrayImage luminance(const FloatImage &image, int maxSide) {
  validate(image);
  if (maxSide < 8)
    throw std::runtime_error("Invalid analysis size.");
  const int step = std::max(
      1, (std::max(image.width, image.height) + maxSide - 1) / maxSide);
  GrayImage out{
      (image.width + step - 1) / step, (image.height + step - 1) / step, {}};
  out.pixels.resize(std::size_t(out.width) * out.height);
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x) {
      double sum = 0;
      int n = 0;
      for (int yy = y * step; yy < std::min((y + 1) * step, image.height); ++yy)
        for (int xx = x * step; xx < std::min((x + 1) * step, image.width);
             ++xx) {
          const auto p = (std::size_t(yy) * image.width + xx) * 3;
          sum += image.rgb[p] * .2126 + image.rgb[p + 1] * .7152 +
                 image.rgb[p + 2] * .0722;
          ++n;
        }
      out.pixels[std::size_t(y) * out.width + x] = float(sum / n);
    }
  return out;
}
double qualityScore(const GrayImage &image) {
  validateGray(image);
  // Gradient energy after a small box filter reduces sensitivity to
  // single-pixel noise.
  if (image.width < 5 || image.height < 5)
    return 0;
  const auto smooth = [&](int x, int y) {
    double sum = 0;
    for (int j = -1; j <= 1; ++j)
      for (int i = -1; i <= 1; ++i)
        sum += image.pixels[std::size_t(y + j) * image.width + x + i];
    return sum / 9;
  };
  double score = 0;
  for (int y = 2; y < image.height - 2; ++y)
    for (int x = 2; x < image.width - 2; ++x) {
      const double dx = smooth(x + 1, y) - smooth(x - 1, y),
                   dy = smooth(x, y + 1) - smooth(x, y - 1);
      score += dx * dx + dy * dy;
    }
  return score / ((image.width - 4) * (image.height - 4));
}
double frameQuality(const FloatImage &image) {
  validate(image);
  const int step =
      std::max(1, (std::max(image.width, image.height) + 511) / 512);
  const auto gray = [&](int x, int y) {
    const auto p = (std::size_t(y) * image.width + x) * 3;
    return image.rgb[p] * .2126 + image.rgb[p + 1] * .7152 +
           image.rgb[p + 2] * .0722;
  };
  double score = 0;
  int count = 0;
  for (int y = 1; y < image.height - 1; y += step) {
    for (int x = 1; x < image.width - 1; x += step) {
      const double a = gray(x - 1, y - 1), b = gray(x, y - 1),
                   c = gray(x + 1, y - 1);
      const double d = gray(x - 1, y), f = gray(x + 1, y);
      const double g = gray(x - 1, y + 1), h = gray(x, y + 1),
                   i = gray(x + 1, y + 1);
      const double dx = (c + 2 * f + i - a - 2 * d - g) / 8,
                   dy = (g + 2 * h + i - a - 2 * b - c) / 8;
      score += dx * dx + dy * dy;
      ++count;
    }
  }
  return count ? score / count : 0;
}
Translation alignTranslation(const GrayImage &reference,
                             const GrayImage &source,
                             const CheckCancel &check) {
  validateGray(reference);
  validateGray(source);
  if (reference.width != source.width || reference.height != source.height ||
      reference.width < 16 || reference.height < 16 ||
      reference.pixels.size() !=
          std::size_t(reference.width) * reference.height ||
      source.pixels.size() != reference.pixels.size())
    throw std::runtime_error(
        "Alignment needs matching images of at least 16 by 16 pixels.");
  std::vector<GrayImage> a{reference}, b{source};
  while (std::min(a.back().width, a.back().height) >= 64) {
    a.push_back(half(a.back()));
    b.push_back(half(b.back()));
  }
  int dx = 0, dy = 0;
  double best = -1;
  for (int level = int(a.size()) - 1; level >= 0; --level) {
    const int radius =
        level == int(a.size()) - 1
            ? std::max(2, std::min(a[level].width, a[level].height) / 8)
            : 2;
    const int cx = dx * 2, cy = dy * 2;
    best = -1;
    for (int y = cy - radius; y <= cy + radius; ++y) {
      checkpoint(check);
      for (int x = cx - radius; x <= cx + radius; ++x) {
        const double v = correlation(a[level], b[level], x, y);
        if (v > best) {
          best = v;
          dx = x;
          dy = y;
        }
      }
    }
  }
  const auto refine = [&](bool horizontal) {
    const double minus =
        correlation(reference, source, dx - (horizontal ? 1 : 0),
                    dy - (horizontal ? 0 : 1));
    const double plus =
        correlation(reference, source, dx + (horizontal ? 1 : 0),
                    dy + (horizontal ? 0 : 1));
    const double divisor = minus - 2 * best + plus;
    return divisor < -1e-9 ? std::clamp(.5 * (minus - plus) / divisor, -.5, .5)
                           : 0.;
  };
  return {dx + refine(true), dy + refine(false), best};
}
namespace {
GrayImage detailPatch(const FloatImage &image, int left, int top, int width,
                      int height) {
  GrayImage out{width, height, std::vector<float>(std::size_t(width) * height)};
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      const auto p = (std::size_t(y + top) * image.width + x + left) * 3;
      out.pixels[std::size_t(y) * width + x] = image.rgb[p] * .2126f +
                                               image.rgb[p + 1] * .7152f +
                                               image.rgb[p + 2] * .0722f;
    }
  return out;
}
} // namespace
AlignmentReference makeAlignmentReference(const FloatImage &image) {
  validate(image);
  AlignmentReference ref;
  ref.coarse = luminance(image);
  ref.width = image.width;
  ref.height = image.height;
  ref.sampleStep =
      std::max(1, (std::max(image.width, image.height) + 255) / 256);
  // Locate a detailed patch on the subject, even when the planet is off-center.
  int bestX = ref.coarse.width / 2, bestY = ref.coarse.height / 2;
  double best = -1;
  for (int y = 2; y < ref.coarse.height - 2; ++y)
    for (int x = 2; x < ref.coarse.width - 2; ++x) {
      const auto p = std::size_t(y) * ref.coarse.width + x;
      const double dx = ref.coarse.pixels[p + 1] - ref.coarse.pixels[p - 1];
      const double dy = ref.coarse.pixels[p + ref.coarse.width] -
                        ref.coarse.pixels[p - ref.coarse.width];
      if (dx * dx + dy * dy > best) {
        best = dx * dx + dy * dy;
        bestX = x;
        bestY = y;
      }
    }
  const int w = std::min(192, image.width), h = std::min(192, image.height);
  ref.originX = std::clamp(bestX * ref.sampleStep - w / 2, 0, image.width - w);
  ref.originY = std::clamp(bestY * ref.sampleStep - h / 2, 0, image.height - h);
  ref.detail = detailPatch(image, ref.originX, ref.originY, w, h);
  return ref;
}
Translation alignToReference(const AlignmentReference &ref,
                             const FloatImage &image,
                             const CheckCancel &check) {
  validate(image);
  if (image.width != ref.width || image.height != ref.height)
    throw std::runtime_error("Source dimensions changed during registration.");
  auto offset = alignTranslation(ref.coarse, luminance(image), check);
  offset.x *= ref.sampleStep;
  offset.y *= ref.sampleStep;
  if (offset.correlation < .5)
    return offset;
  const int left = std::clamp(ref.originX + int(std::lround(offset.x)), 0,
                              image.width - ref.detail.width);
  const int top = std::clamp(ref.originY + int(std::lround(offset.y)), 0,
                             image.height - ref.detail.height);
  const auto patch =
      detailPatch(image, left, top, ref.detail.width, ref.detail.height);
  const auto fine = alignTranslation(ref.detail, patch, check);
  const double x = left - ref.originX + fine.x, y = top - ref.originY + fine.y;
  // Reject disagreement rather than blur the stack with an ambiguous match.
  if (fine.correlation < .5 ||
      std::abs(x - offset.x) > std::max(2, 2 * ref.sampleStep) ||
      std::abs(y - offset.y) > std::max(2, 2 * ref.sampleStep))
    return {0, 0, -1};
  return {x, y, std::min(offset.correlation, fine.correlation)};
}
MeanStack::MeanStack(int width, int height) {
  if (width <= 0 || height <= 0 || std::uint64_t(width) * height > 16000000)
    throw std::runtime_error("Invalid stack size.");
  width_ = width;
  height_ = height;
  sum_.resize(std::size_t(width) * height * 3, 0);
  right_ = width;
  bottom_ = height;
}
void MeanStack::add(const FloatImage &image, double dx, double dy,
                    const CheckCancel &check) {
  validate(image);
  if (image.width != width_ || image.height != height_ || !std::isfinite(dx) ||
      !std::isfinite(dy) || std::abs(dx) >= image.width ||
      std::abs(dy) >= image.height)
    throw std::runtime_error("Invalid stack translation.");
  left_ = std::max(left_, int(std::ceil(-dx)));
  top_ = std::max(top_, int(std::ceil(-dy)));
  right_ = std::min(right_, int(std::floor(image.width - 1 - dx)) + 1);
  bottom_ = std::min(bottom_, int(std::floor(image.height - 1 - dy)) + 1);
  if (right_ <= left_ || bottom_ <= top_)
    throw std::runtime_error("Aligned images have no common area.");
  for (int y = top_; y < bottom_; ++y) {
    checkpoint(check);
    const double sy = y + dy;
    const int y0 = int(std::floor(sy)), y1 = std::min(y0 + 1, image.height - 1);
    const float fy = float(sy - y0);
    for (int x = left_; x < right_; ++x) {
      const double sx = x + dx;
      const int x0 = int(std::floor(sx)),
                x1 = std::min(x0 + 1, image.width - 1);
      const float fx = float(sx - x0);
      for (int c = 0; c < 3; ++c) {
        const auto at = [&](int xx, int yy) {
          return image.rgb[(std::size_t(yy) * image.width + xx) * 3 + c];
        };
        sum_[(std::size_t(y) * image.width + x) * 3 + c] +=
            (at(x0, y0) * (1 - fx) + at(x1, y0) * fx) * (1 - fy) +
            (at(x0, y1) * (1 - fx) + at(x1, y1) * fx) * fy;
      }
    }
  }
  ++count_;
}
void MeanStack::restrictSupport(int left, int top, int right, int bottom) {
  left_ = std::max(left_, left); top_ = std::max(top_, top);
  right_ = std::min(right_, right); bottom_ = std::min(bottom_, bottom);
  if (left_ < 0 || top_ < 0 || right_ > width_ || bottom_ > height_ || right_ <= left_ || bottom_ <= top_)
    throw std::runtime_error("Aligned images have no common area.");
}
FloatImage MeanStack::finish() const {
  if (!count_)
    throw std::runtime_error("No frames were stacked.");
  FloatImage out{right_ - left_, bottom_ - top_, {}};
  out.rgb.resize(std::size_t(out.width) * out.height * 3);
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x)
      for (int c = 0; c < 3; ++c)
        out.rgb[(std::size_t(y) * out.width + x) * 3 + c] =
            float(sum_[(std::size_t(y + top_) * width_ + x + left_) * 3 + c] /
                  count_);
  return out;
}
FloatImage wavelets(const FloatImage &image, const std::array<float, 6> &gains,
                    float denoise, const std::array<float, 3> &color,
                    float gamma, const CheckCancel &check, ComputePreference preference, ComputeReport* report,const PreviewTarget* preview) {
  validate(image, astro::domain::maximumImagePixels);
  if (!std::isfinite(gamma) || gamma < .1f || gamma > 5 ||
      !std::isfinite(denoise) || denoise < 0 || denoise > 1)
    throw std::runtime_error("Invalid wavelet settings.");
  for (std::size_t i = 0; i < gains.size(); ++i)
    if (!std::isfinite(gains[i]) || gains[i] < 0 || gains[i] > (i == 0 ? 50 : i < 3 ? 20 : 10))
      throw std::runtime_error("Invalid wavelet gain.");
  for (float v : color)
    if (!std::isfinite(v) || v < 0 || v > 4)
      throw std::runtime_error("Invalid color gain.");
  if(preview)validatePreviewTarget(image,*preview);
  checkpoint(check);
  if(detail::requestMetal(image.rgb.size()/3,preference,report,std::any_of(gains.begin(),gains.end(),[](float v){return v!=0;})?512*512:1024*1024)) {
    FloatImage accelerated;std::string reason;
    if(detail::metalWavelets(image,gains,denoise,color,gamma,accelerated,check,reason,preview,report)) {
      if(report)report->backend=ComputeBackend::Metal;return accelerated;
    }
    if(report)report->fallbackReason=std::move(reason);
  }
  FloatImage out = image;
  constexpr float weights[5] = {1.f / 16, 4.f / 16, 6.f / 16, 4.f / 16,
                                1.f / 16};
  int last = -1;
  for (int i = 0; i < 6; ++i)
    if (gains[i] != 0)
      last = i;
  std::vector<float> current, temp, blurred;
  if (last >= 0) {
    current = image.rgb;
    temp.resize(current.size());
    blurred.resize(current.size());
  }
  for (int level = 0; level <= last; ++level) {
    const int step = 1 << level;
    for (int pass = 0; pass < 2; ++pass) {
      const auto &src = pass == 0 ? current : temp;
      auto &dst = pass == 0 ? temp : blurred;
      for (int y = 0; y < image.height; ++y) {
        checkpoint(check);
        for (int x = 0; x < image.width; ++x)
          for (int c = 0; c < 3; ++c) {
            float sum = 0;
            for (int k = -2; k <= 2; ++k) {
              const int xx = std::clamp(x + (pass == 0 ? k * step : 0), 0,
                                        image.width - 1);
              const int yy = std::clamp(y + (pass == 1 ? k * step : 0), 0,
                                        image.height - 1);
              sum += src[(std::size_t(yy) * image.width + xx) * 3 + c] *
                     weights[k + 2];
            }
            dst[(std::size_t(y) * image.width + x) * 3 + c] = sum;
          }
      }
    }
    for (std::size_t i = 0; i < current.size(); ++i) {
      if (i % (std::size_t(image.width) * 3) == 0)
        checkpoint(check);
      const float detail = current[i] - blurred[i];
      out.rgb[i] +=
          gains[level] *
          std::copysign(std::max(0.f, std::abs(detail) - denoise), detail);
    }
    current.swap(blurred);
  }
  for (std::size_t i = 0; i < out.rgb.size(); ++i) {
    if (i % (std::size_t(image.width) * 3) == 0)
      checkpoint(check);
    out.rgb[i] =
        std::pow(std::max(0.f, out.rgb[i] * color[i % 3]), 1.f / gamma);
  }
  if(preview)renderRgb8(out,preview->pixels,preview->bytesPerLine,check,ComputePreference::Cpu);
  return out;
}
} // namespace astro::processing
