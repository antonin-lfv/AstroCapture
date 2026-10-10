#include "Check.hpp"
#include <astro/recording/AstroImage.hpp>
#include <cmath>
#include <filesystem>
#include <random>
using astro::recording::CelestialWcs;
int main() {
  CelestialWcs w{{359.8, 72}, {513.5, 385.5}, {-.0003, .00008, .00004, .0003}};
  w.sipOrder = 3;
  w.sipA[12] = 2e-5;
  w.sipA[7] = -1e-5;
  w.sipA[3] = 1e-8;
  w.sipB[2] = -2e-5;
  w.sipB[18] = -8e-9;
  ASTRO_TEST_CHECK(w.valid());
  // Independent SIP forward evaluation compared with an undistorted TAN map.
  CelestialWcs tan = w;
  tan.sipOrder = 0;
  tan.sipA.fill(0);
  tan.sipB.fill(0);
  std::vector<std::array<double, 6>> maps{{1, 0, -35, 0, 1, -27},
                                          {0, -.5, 400, .5, 0, -12},
                                          {-.8, .2, 900, .15, 1.2, -50},
                                          {1.1, 0, .05, 0, .7, -.15}};
  for (int y = 0; y <= 8; ++y)
    for (int x = 0; x <= 8; ++x) {
      const double px = x * 1023 / 8., py = y * 767 / 8.,
                   u = px + 1 - w.crpix[0], v = py + 1 - w.crpix[1];
      const auto expected =
          tan.sky(px + 2e-5 * u * u - 1e-5 * u * v + 1e-8 * v * v * v,
                  py - 2e-5 * v * v - 8e-9 * u * u * u);
      const auto sky = w.sky(px, py);
      ASTRO_TEST_CHECK(sky && expected);
      ASTRO_TEST_CHECK(
          std::abs(std::remainder((*sky)[0] - (*expected)[0], 360.)) < 1e-10 &&
          std::abs((*sky)[1] - (*expected)[1]) < 1e-10);
      const auto inverse = w.pixel((*sky)[0], (*sky)[1]);
      ASTRO_TEST_CHECK(
          inverse && std::hypot((*inverse)[0] - px, (*inverse)[1] - py) < 1e-6);
      for (const auto &map : maps) {
        const auto transformed = w.remap(map);
        ASTRO_TEST_CHECK(transformed.valid());
        const auto after = transformed.sky(map[0] * px + map[1] * py + map[2],
                                           map[3] * px + map[4] * py + map[5]);
        ASTRO_TEST_CHECK(
            after &&
            std::abs(std::remainder((*sky)[0] - (*after)[0], 360.)) < 1e-10 &&
            std::abs((*sky)[1] - (*after)[1]) < 1e-10);
      }
    }
  const auto path =
      std::filesystem::temp_directory_path() / "astro-sip-roundtrip.fits";
  std::filesystem::remove(path);
  astro::recording::AstroImage image;
  image.width = 10;
  image.height = 8;
  image.channels = 3;
  image.samples.resize(240);
  for (int n = 0; n < 240; ++n)
    image.samples[n] = float(n / 73. - .2);
  image.wcs = w;
  astro::recording::writeAstroFits(path, image);
  const auto read = astro::recording::readAstroImage(path, 1);
  const auto header = astro::recording::readCelestialWcs(path);
  ASTRO_TEST_CHECK(read.samples == image.samples && read.wcs && header &&
                   read.wcs->sipOrder == 3 && read.wcs->sipA == w.sipA &&
                   read.wcs->sipB == w.sipB);
  std::filesystem::remove(path);
  w.sipA[0] = 1;
  ASTRO_TEST_CHECK(!w.valid());
}
