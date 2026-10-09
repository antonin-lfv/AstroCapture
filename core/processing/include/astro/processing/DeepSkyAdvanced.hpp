#pragma once
#include <astro/processing/DeepSky.hpp>

namespace astro::processing {
// Independent implementation of the published GHS equations (Payne /
// Cranfield).
struct GhsOptions {
  double strength{}, localIntensity{}, symmetry{}, shadows{}, highlights{1};
  bool inverse{};
  int mode{}; // 0: independent RGB; 1: shared weighted luminance with RGB blend
              // clipping
};
class GhsCurve {
public:
  explicit GhsCurve(const GhsOptions &);
  double operator()(double) const;

private:
  double base(double) const, derivative(double) const, raw(double) const;
  GhsOptions options_;
  double d_{}, low_{}, range_{1};
};
struct MaskStroke {
  std::vector<std::array<double,2>> points; // Normalized image coordinates.
  double radius{.02},hardness{.7},opacity{1},value{};
};
struct MaskOptions {
  int type{}; // 0: none, 1: luminance range, 2: radial, 3: detected stars, 4: painted, 5: imported
  double low{}, high{1}, feather{.05}, centerX{.5}, centerY{.5}, radius{.25},
      starRadius{2};
  bool invert{};
  bool baseWhite{true};
  double blurRadius{}; // Gaussian sigma in image pixels.
  std::vector<MaskStroke> strokes;
  std::vector<float> raster; // Imported mask, exact image dimensions, [0,1].
};
std::vector<float> makeMask(const FloatImage &, const MaskOptions &,
                            const CheckCancel & = {});
void blendMask(const FloatImage &original, FloatImage &result,
               const std::vector<float> &, const CheckCancel & = {});
struct PsfOptions {
  int profile{};
  double fwhm{3}, beta{3};
};
struct PsfEstimate {
  double fwhm{}, scatter{};
  int stars{};
};
PsfEstimate estimatePsf(const FloatImage &, const CheckCancel & = {});
std::vector<float> makePsf(const PsfOptions &, int *size = nullptr);
struct DeconvolutionOptions {
  PsfOptions psf;
  int iterations{10};
  double damping{.001}, blend{1}, stopping{.0001};
};
struct DeconvolutionResult {
  FloatImage image;
  int iterations{};
  double relativeChange{};
};
DeconvolutionResult deconvolve(const FloatImage &, const DeconvolutionOptions &,
                               const CheckCancel & = {}, const Progress & = {});
struct DenoiseOptions {
  int scales{3};
  double threshold{3}, amount{.5}, chroma{.5};
};
struct DenoiseResult {
  FloatImage image;
  double noiseBefore{}, noiseAfter{};
};
DenoiseResult denoiseMultiscale(const FloatImage &, const DenoiseOptions &,
                                const CheckCancel & = {});
struct PhotometricReference {
  double x{}, y{};
  std::array<double, 3> flux{1, 1, 1};
};
struct PhotometryOptions {
  double aperture{5}, inner{8}, outer{12}, saturation{1};
};
struct PhotometryResult {
  FloatImage image;
  std::array<double, 3> gains{1, 1, 1};
  int accepted{}, rejected{};
  double scatter{};
  int edgeRejected{},saturatedRejected{},signalRejected{},outlierRejected{};
};
PhotometryResult calibratePhotometry(const FloatImage &,
                                     const std::vector<PhotometricReference> &,
                                     const PhotometryOptions &,
                                     const CheckCancel & = {});
} // namespace astro::processing
