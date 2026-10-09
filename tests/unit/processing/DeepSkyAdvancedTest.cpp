#include "Check.hpp"
#include <astro/processing/DeepSkyAdvanced.hpp>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
using namespace astro::processing;
int main() {
  // Analytic harmonic GHS fixture: D=9, SP=0 => 10x/(1+9x).
  GhsOptions o;
  o.strength = std::log(10.);
  o.localIntensity = 1;
  GhsCurve curve(o);
  for (int n = 0; n <= 1000; ++n) {
    const double x = n / 1000.;
    ASTRO_TEST_CHECK(std::abs(curve(x) - 10 * x / (1 + 9 * x)) < 1e-12);
  }
  for (double b : {-5., -1.000001, -1., -.999999, -.1, 0., .000001, 1., 15.}) {
    o.localIntensity = b;
    o.symmetry = .3;
    o.shadows = .05;
    o.highlights = .8;
    o.strength = 2;
    GhsCurve forward(o);
    o.inverse = true;
    GhsCurve inverse(o);
    o.inverse = false;
    double last = -1;
    for (int n = 0; n <= 1000; ++n) {
      const double x = n / 1000., v = forward(x);
      ASTRO_TEST_CHECK(std::isfinite(v) && v >= last &&
                       std::abs(inverse(v) - x) < 1e-8);
      last = v;
    }
    ASTRO_TEST_CHECK(forward(0) == 0 && forward(1) == 1);
  }
  o.strength = 0;
  GhsCurve identity(o);
  ASTRO_TEST_CHECK(identity(.234) == .234);
  bool refused = false;
  o.shadows = .4;
  try {
    GhsCurve bad(o);
  } catch (const std::exception &) {
    refused = true;
  }
  ASTRO_TEST_CHECK(refused);
  FloatImage colors{2, 1, {.1f, .3f, .8f, .8f, .2f, .1f}};
  Finish f;
  f.ghs = true;
  f.ghsStrength = 2;
  f.ghsMode = 1;
  auto stretched = finishDeepSky(colors, f);
  for (float v : stretched.rgb)
    ASTRO_TEST_CHECK(v >= 0 && v <= 1 && std::isfinite(v));
  // Weighted mask: explicit exact protection and half-weight interpolation.
  auto edited = colors;
  for (auto &v : edited.rgb)
    v += .1f;
  blendMask(colors, edited, {0, .5});
  for (int c = 0; c < 3; ++c) {
    ASTRO_TEST_CHECK(edited.rgb[c] == colors.rgb[c]);
    ASTRO_TEST_CHECK(std::abs(edited.rgb[3 + c] - colors.rgb[3 + c] - .05) <
                     1e-6);
  }
  FloatImage field{96, 96, std::vector<float>(96 * 96 * 3, .1f)};
  MaskOptions m;
  m.type = 2;
  m.radius = .15;
  m.feather = .1;
  const auto mask = makeMask(field, m);
  ASTRO_TEST_CHECK(mask[48 * 96 + 48] == 1 && mask[0] == 0);
  m.invert = true;
  const auto inverseMask = makeMask(field, m);
  for (std::size_t i = 0; i < mask.size(); ++i)
    ASTRO_TEST_CHECK(std::abs(mask[i] + inverseMask[i] - 1) < 1e-6);
  // Normalized, centered and symmetric kernels for both profiles.
  for (int profile : {0, 1}) {
    int n;
    const auto psf = makePsf({profile, 3, 3}, &n);
    ASTRO_TEST_CHECK(n % 2 == 1 &&
                     std::abs(std::accumulate(psf.begin(), psf.end(), 0.) - 1) <
                         1e-6);
    for (std::size_t i = 0; i < psf.size(); ++i)
      ASTRO_TEST_CHECK(psf[i] == psf[psf.size() - 1 - i]);
  }
  DeconvolutionOptions rl;
  rl.iterations = 8;
  rl.damping = 0;
  auto constant = deconvolve(field, rl);
  for (float v : constant.image.rgb)
    ASTRO_TEST_CHECK(std::abs(v - .1) < 1e-5);
  FloatImage starField{128, 128, std::vector<float>(128 * 128 * 3, .01f)};
  const std::array<std::array<int, 2>, 5> centers{
      {{35, 35}, {90, 35}, {35, 90}, {90, 90}, {64, 64}}};
  for (int y = 0; y < 128; ++y)
    for (int x = 0; x < 128; ++x) {
      double v = .01;
      for (auto p : centers)
        v +=
            .3 * std::exp(-((x - p[0]) * (x - p[0]) + (y - p[1]) * (y - p[1])) /
                          4.5);
      for (int c = 0; c < 3; ++c)
        starField.rgb[(y * 128 + x) * 3 + c] = float(v);
    }
  const auto estimate = estimatePsf(starField);
  ASTRO_TEST_CHECK(estimate.stars >= 3 &&
                   std::abs(estimate.fwhm - 2.354820045 * 1.5) < .05);
  rl.psf.fwhm = estimate.fwhm;
  rl.iterations = 5;
  const auto restored = deconvolve(starField, rl);
  ASTRO_TEST_CHECK(restored.iterations == 5 &&
                   restored.image.rgb[(64 * 128 + 64) * 3] >
                       starField.rgb[(64 * 128 + 64) * 3]);
  for (float v : restored.image.rgb)
    ASTRO_TEST_CHECK(std::isfinite(v) && v >= 0);
  auto negative = field;
  negative.rgb[0] = -.01;
  refused = false;
  try {
    deconvolve(negative, rl);
  } catch (const std::exception &) {
    refused = true;
  }
  ASTRO_TEST_CHECK(refused);
  int calls = 0;
  refused = false;
  try {
    deconvolve(starField, rl, [&] {
      if (++calls > 200)
        throw std::logic_error("cancel");
    });
  } catch (const std::logic_error &) {
    refused = true;
  }
  ASTRO_TEST_CHECK(refused);
  // Synthetic independent noise: reduction must improve error to known
  // constant.
  auto noisy = field;
  std::mt19937 random(42);
  std::normal_distribution<float> gaussian(0, .005);
  for (auto &v : noisy.rgb)
    v += gaussian(random);
  DenoiseOptions dn;
  dn.amount = .8;
  dn.chroma = .8;
  const auto cleaned = denoiseMultiscale(noisy, dn);
  double before = 0, after = 0;
  for (std::size_t i = 0; i < noisy.rgb.size(); ++i) {
    before += std::pow(noisy.rgb[i] - .1, 2);
    after += std::pow(cleaned.image.rgb[i] - .1, 2);
  }
  ASTRO_TEST_CHECK(after < before * .4 &&
                   cleaned.noiseAfter < cleaned.noiseBefore);
  dn.amount = dn.chroma = 0;
  const auto neutral = denoiseMultiscale(noisy, dn);
  ASTRO_TEST_CHECK(neutral.image.rgb == noisy.rgb);
  // Matched catalogue with known camera color response and a local background.
  FloatImage rgb = starField;
  for (std::size_t i = 0; i < rgb.rgb.size(); i += 3) {
    rgb.rgb[i] *= .5f;
    rgb.rgb[i + 2] *= 2;
  }
  std::vector<PhotometricReference> references;
  for (auto p : centers)
    references.push_back({double(p[0]), double(p[1]), {1, 1, 1}});
  auto calibrated = calibratePhotometry(rgb, references, {});
  ASTRO_TEST_CHECK(calibrated.accepted == 5 &&
                   std::abs(calibrated.gains[0] - 2) < 1e-5 &&
                   std::abs(calibrated.gains[2] - .5) < 1e-5);
  for (std::size_t i = 0; i < calibrated.image.rgb.size(); i += 3)
    ASTRO_TEST_CHECK(std::abs(calibrated.image.rgb[i] -
                              calibrated.image.rgb[i + 1]) < 1e-6 &&
                     std::abs(calibrated.image.rgb[i + 2] -
                              calibrated.image.rgb[i + 1]) < 1e-6);
  references.push_back({0, 0, {1, 1, 1}});
  calibrated = calibratePhotometry(rgb, references, {});
  ASTRO_TEST_CHECK(calibrated.rejected == 1 && calibrated.accepted == 5);
  auto saturated = rgb;
  saturated.rgb[(35 * 128 + 35) * 3] = 1;
  calibrated = calibratePhotometry(saturated, references, {});
  ASTRO_TEST_CHECK(calibrated.accepted == 4 && calibrated.rejected == 2);
  references.resize(2);
  refused = false;
  try {
    calibratePhotometry(rgb, references, {});
  } catch (const std::exception &) {
    refused = true;
  }
  ASTRO_TEST_CHECK(refused);
  std::cout << "OK\n";
}
