#include "Check.hpp"
#include <astro/processing/Astrometry.hpp>
#include <astro/recording/AstroImage.hpp>
#include <filesystem>
#include <limits>
#include <random>
using namespace astro::processing;
namespace recording = astro::recording;
int main() {
  recording::CelestialWcs w{
      {359.98, 82}, {513.5, 385.5}, {-.0002, .0001, .0001, .0002}};
  for (auto p : std::vector<std::array<double, 2>>{
           {0, 0}, {512.5, 384.5}, {1023, 767}, {13.125, 73.7}}) {
    const auto sky = w.sky(p[0], p[1]);
    ASTRO_TEST_CHECK(sky);
    const auto pixel = w.pixel((*sky)[0], (*sky)[1]);
    ASTRO_TEST_CHECK(pixel &&
                     std::hypot((*pixel)[0] - p[0], (*pixel)[1] - p[1]) < 1e-8);
  }
  auto centre = w.sky(w.crpix[0] - 1, w.crpix[1] - 1);
  ASTRO_TEST_CHECK(centre && std::abs((*centre)[0] - 359.98) < 1e-10 &&
                   std::abs((*centre)[1] - 82) < 1e-10);
  ASTRO_TEST_CHECK(!w.pixel(179.98, -82));
  // Independent east/north small-angle reference, not only inverse round trips.
  recording::CelestialWcs equator{{0, 0}, {1, 1}, {1. / 3600, 0, 0, 1. / 3600}};
  auto east = equator.sky(1, 0);
  ASTRO_TEST_CHECK(
      east &&
      std::abs((*east)[0] - std::atan(1. / 3600 * 3.141592653589793 / 180) *
                                180 / 3.141592653589793) < 1e-12);
  std::mt19937 random(42);
  std::uniform_real_distribution<double> xx(45, 980), yy(45, 720), aa(.03, .3);
  for (bool mirror : {false, true}) {
    FloatImage image{1024, 768, std::vector<float>(1024 * 768 * 3, .001f)};
    const double angle = .39, scale = 1.1 / 3600;
    recording::CelestialWcs actual{
        {83.822, -5.390},
        {512.5, 384.5},
        {-scale * std::cos(angle) * (mirror ? -1 : 1), scale * std::sin(angle),
         scale * std::sin(angle) * (mirror ? -1 : 1), scale * std::cos(angle)}};
    std::vector<CatalogStar> catalog;
    std::vector<std::array<double, 2>> positions;
    for (int n = 0; n < 55; ++n) {
      double x = xx(random), y = yy(random);
      if (std::any_of(positions.begin(), positions.end(), [&](auto p) {
            return std::hypot(x - p[0], y - p[1]) < 25;
          })) {
        --n;
        continue;
      }
      positions.push_back({x, y});
      const double amp = aa(random);
      auto sky = *actual.sky(x, y);
      catalog.push_back({std::to_string(3000000000000000000ULL + n), sky[0],
                         sky[1], -std::log(amp) * 2.5, 5000, true});
      for (int py = int(y) - 10; py <= int(y) + 10; ++py)
        for (int px = int(x) - 10; px <= int(x) + 10; ++px) {
          const auto offset = (std::size_t(py) * image.width + px) * 3;
          const double v =
              amp * std::exp(-((px - x) * (px - x) + (py - y) * (py - y)) / 5);
          for (int c = 0; c < 3; ++c)
            image.rgb[offset + c] += float(v * (c == 0 ? .8 : c == 1 ? 1 : .6));
        }
    }
    for (int n = 0; n < 20; ++n) {
      auto sky = *actual.sky(xx(random), yy(random));
      catalog.push_back({std::to_string(4000000000000000000ULL + n), sky[0],
                         sky[1], 10 + n * .1, 6000, false});
    }
    std::sort(catalog.begin(), catalog.end(), [](const auto &a, const auto &b) {
      return a.magnitude < b.magnitude;
    });
    recording::CelestialWcs seed{
        {83.84, -5.40}, {512.5, 384.5}, {-1.3 / 3600, 0, 0, 1.3 / 3600}};
    auto solved = solveAstrometry(image, catalog, seed);
    ASTRO_TEST_CHECK(solved.matches.size() >= 40 && solved.rms < .4 &&
                     solved.coverage > .5);
    for (auto p : positions) {
      auto sky = *actual.sky(p[0], p[1]);
      auto q = solved.wcs.pixel(sky[0], sky[1]);
      ASTRO_TEST_CHECK(q && std::hypot((*q)[0] - p[0], (*q)[1] - p[1]) < .5);
    }
    bool cancelled = false;
    try {
      solveAstrometry(image, catalog, seed,
                      [] { throw std::runtime_error("cancel"); });
    } catch (const std::runtime_error &) {
      cancelled = true;
    }
    ASTRO_TEST_CHECK(cancelled);
    for (auto &s : catalog)
      s.dec += 12;
    bool rejected = false;
    try {
      solveAstrometry(image, catalog, seed);
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    ASTRO_TEST_CHECK(rejected);
  }
  Spectrum constant{{336, 1}, {500, 1}, {1020, 1}};
  SpectralResponse triangle{
      {400, {0, 0, 0}}, {500, {1, .5, .25}}, {600, {0, 0, 0}}};
  auto flux = spectralFlux(constant, triangle);
  ASTRO_TEST_CHECK(std::abs(flux[0] - 50000) < 1e-8 &&
                   std::abs(flux[1] - 25000) < 1e-8 &&
                   std::abs(flux[2] - 12500) < 1e-8);
  auto neutral = solarSpectrum();
  auto colors = referenceColors(neutral, standardResponse(), neutral);
  for (double v : colors)
    ASTRO_TEST_CHECK(std::abs(v - 1) < 1e-12);
  auto hot = referenceColors(temperatureSpectrum(12000), standardResponse(),
                             neutral),
       cool = referenceColors(temperatureSpectrum(3500), standardResponse(),
                              neutral);
  ASTRO_TEST_CHECK(hot[2] > cool[2] && hot[0] < cool[0]);
  // End-to-end PCC fixture: spectra -> synthetic photon colors -> camera
  // response -> aperture photometry. Recover the known relative channel gains.
  FloatImage field{512, 512, std::vector<float>(512 * 512 * 3)};
  for (std::size_t n = 0; n < field.rgb.size(); ++n)
    field.rgb[n] = float(.005 + .002 * (n % 3));
  std::vector<PhotometricReference> references;
  const std::array<double, 3> camera{.5, 1.3, 2};
  for (int n = 0; n < 36; ++n) {
    const double x = 40 + (n % 6) * 80, y = 40 + (n / 6) * 80;
    auto color = referenceColors(temperatureSpectrum(3000 + n * 200),
                                 standardResponse(), neutral);
    references.push_back({x, y, color});
    for (int py = int(y) - 15; py <= int(y) + 15; ++py)
      for (int px = int(x) - 15; px <= int(x) + 15; ++px) {
        const double signal =
            .035 * std::exp(-((px - x) * (px - x) + (py - y) * (py - y)) / 4.);
        for (int c = 0; c < 3; ++c)
          field.rgb[(std::size_t(py) * 512 + px) * 3 + c] +=
              float(signal * color[c] * camera[c]);
      }
  }
  auto calibrated = calibratePhotometry(field, references, {});
  ASTRO_TEST_CHECK(
      calibrated.accepted >= 30 && calibrated.scatter < 1e-5 &&
      std::abs(calibrated.gains[0] - camera[1] / camera[0]) < 1e-5 &&
      std::abs(calibrated.gains[2] - camera[1] / camera[2]) < 1e-5);
  // A pixel-coordinate affine map must preserve the same sky location after
  // crop, rotation, mirror and resize; includes half-pixel centre offsets.
  std::array<double, 6> map{0, -.5, 399.25, .5, 0, -12.25};
  auto remapped = w.remap(map);
  auto before = w.sky(140.75, 300.25),
       after = remapped.sky(map[0] * 140.75 + map[1] * 300.25 + map[2],
                            map[3] * 140.75 + map[4] * 300.25 + map[5]);
  ASTRO_TEST_CHECK(before && after &&
                   std::abs(std::remainder((*before)[0] - (*after)[0], 360.)) <
                       1e-12 &&
                   std::abs((*before)[1] - (*after)[1]) < 1e-12);
  bool bad = false;
  triangle.front().response[0] = .1;
  try {
    spectralFlux(constant, triangle);
  } catch (const std::exception &) {
    bad = true;
  }
  ASTRO_TEST_CHECK(bad);
  // FITS float32 + WCS round-trip preserves negative/HDR samples and row order.
  const auto path =
      std::filesystem::temp_directory_path() / "astro-wcs-roundtrip.fits";
  std::filesystem::remove(path);
  recording::AstroImage fits;
  fits.width = 4;
  fits.height = 3;
  fits.channels = 3;
  fits.bits = 32;
  fits.wcs = w;
  fits.samples.resize(36);
  for (int n = 0; n < 36; ++n)
    fits.samples[n] = float(n / 17. - .2);
  recording::writeAstroFits(path, fits);
  auto read = recording::readAstroImage(path, 1);
  ASTRO_TEST_CHECK(read.wcs && read.samples == fits.samples &&
                   read.wcs->crpix == w.crpix && read.wcs->cd == w.cd);
  std::filesystem::remove(path);
}
