#pragma once
#include <array>
#include <cmath>
#include <optional>
namespace astro::recording {
// FITS TAN, ICRS. CRPIX is one-based; public pixel coordinates are zero-based
// in the stored row order (the FITS writer never reverses image rows).
struct CelestialWcs {
  std::array<double, 2> crval{}, crpix{};
  std::array<double, 4> cd{};
  bool valid() const {
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
    return std::array<double, 2>{
        crpix[0] - 1 + (cd[3] * (*p)[0] - cd[1] * (*p)[1]) / det,
        crpix[1] - 1 + (-cd[2] * (*p)[0] + cd[0] * (*p)[1]) / det};
  }
  std::optional<std::array<double, 2>> sky(double x, double y) const {
    if (!valid() || !std::isfinite(x) || !std::isfinite(y))
      return {};
    constexpr double r = 3.14159265358979323846 / 180.;
    x += 1 - crpix[0];
    y += 1 - crpix[1];
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
    return out;
  }
};
} // namespace astro::recording
