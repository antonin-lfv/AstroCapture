#include "CalibrationReferenceData.hpp"
#include <algorithm>
#include <astro/processing/Astrometry.hpp>
#include <limits>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <unordered_map>
namespace astro::processing {
SpectralResponse standardResponse() { return builtinStandardResponse(); }
Spectrum solarSpectrum() { return builtinSolarSpectrum(); }
namespace {
void checked(const CheckCancel &c) {
  if (c)
    c();
}
using Matrix = std::array<double, 6>;
struct Point {
  double x, y;
  int index;
};
struct Triangle {
  std::array<int, 3> vertices;
  double a, b;
};
std::vector<Triangle> triangles(const std::vector<Point> &stars) {
  const int n = std::min(60, int(stars.size()));
  std::vector<Triangle> out;
  auto dist = [&](int a, int b) {
    return std::hypot(stars[a].x - stars[b].x, stars[a].y - stars[b].y);
  };
  for (int i = 0; i < n; ++i)
    for (int j = i + 1; j < n; ++j)
      for (int k = j + 1; k < n; ++k) {
        std::array<std::pair<double, int>, 3> edges{
            {{dist(j, k), i}, {dist(i, k), j}, {dist(i, j), k}}};
        std::sort(edges.begin(), edges.end());
        if (edges[0].first < 15 || edges[2].first < 50 ||
            edges[0].first + edges[1].first < 1.08 * edges[2].first)
          continue;
        out.push_back({{edges[0].second, edges[1].second, edges[2].second},
                       edges[0].first / edges[2].first,
                       edges[1].first / edges[2].first});
      }
  return out;
}
long long cell(int x, int y) {
  return (static_cast<long long>(x) + 1000000) * 2000001 + (y + 1000000);
}
struct Matcher {
  const std::vector<Point> &ref;
  std::unordered_multimap<long long, int> cells;
  explicit Matcher(const std::vector<Point> &r) : ref(r) {
    for (int i = 0; i < int(r.size()); ++i)
      cells.emplace(
          cell(int(std::floor(r[i].x / 16)), int(std::floor(r[i].y / 16))), i);
  }
  std::vector<std::pair<int, int>> match(const std::vector<Point> &src,
                                         const Matrix &m, double limit) const {
    std::vector<std::pair<int, int>> out;
    std::vector<bool> used(ref.size());
    for (int s = 0; s < int(src.size()); ++s) {
      const double x = m[0] * src[s].x + m[1] * src[s].y + m[2],
                   y = m[3] * src[s].x + m[4] * src[s].y + m[5];
      if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1e6 ||
          std::abs(y) > 1e6)
        continue;
      const int cx = int(std::floor(x / 16)), cy = int(std::floor(y / 16));
      int best = -1;
      double d = limit;
      for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy) {
          const auto [first, last] = cells.equal_range(cell(cx + dx, cy + dy));
          for (auto i = first; i != last; ++i) {
            const int r = i->second;
            if (used[r])
              continue;
            const double v = std::hypot(x - ref[r].x, y - ref[r].y);
            if (v < d) {
              best = r;
              d = v;
            }
          }
        }
      if (best >= 0) {
        used[best] = true;
        out.emplace_back(best, s);
      }
    }
    return out;
  }
};
bool plausible(const Matrix &m) {
  const double a = std::hypot(m[0], m[3]), b = std::hypot(m[1], m[4]),
               det = std::abs(m[0] * m[4] - m[1] * m[3]);
  return a > .5 && a < 2 && b > .5 && b < 2 && det > .25 && det < 4 &&
         std::abs(m[0] * m[1] + m[3] * m[4]) / (a * b) < .15 && a / b > .85 &&
         a / b < 1.18;
}
double interpolate(const Spectrum &s, double wavelength) {
  auto i = std::lower_bound(s.begin(), s.end(), wavelength,
                            [](const auto &p, double x) { return p[0] < x; });
  if (i == s.begin())
    return (*i)[1];
  if (i == s.end())
    return s.back()[1];
  const auto a = *(i - 1), b = *i;
  return a[1] + (b[1] - a[1]) * (wavelength - a[0]) / (b[0] - a[0]);
}
} // namespace
AstrometricSolution solveAstrometry(const FloatImage &image,
                                    const std::vector<CatalogStar> &catalog,
                                    const recording::CelestialWcs &seed,
                                    const CheckCancel &check) {
  if (!seed.valid() || catalog.size() < 8 || catalog.size() > 5000)
    throw std::runtime_error("Provide a field centre, pixel scale and at least "
                             "eight catalogue stars.");
  const auto detected = detectStars(image, check, 800);
  if (detected.size() < 8)
    throw std::runtime_error(
        "At least eight detected stars are required for WCS.");
  std::vector<Point> ref, src;
  for (int i = 0; i < int(catalog.size()); ++i)
    if (auto p = seed.pixel(catalog[i].ra, catalog[i].dec))
      ref.push_back({(*p)[0], (*p)[1], i});
  for (int i = 0; i < int(detected.size()); ++i)
    src.push_back({detected[i].x, detected[i].y, i});
  if (ref.size() < 8)
    throw std::runtime_error("Too few catalogue stars in the tangent field.");
  Matcher matcher(ref);
  Matrix best{1, 0, 0, 0, 1, 0};
  int bestCount = int(matcher.match(src, best, 5).size());
  if (bestCount < 12) {
    const auto a = triangles(ref), b = triangles(src);
    std::unordered_multimap<int, const Triangle *> lookup;
    for (const auto &t : a)
      lookup.emplace(int(t.a * 500) * 501 + int(t.b * 500), &t);
    int attempts = 0;
    std::vector<Point> bright(src.begin(),
                              src.begin() + std::min(150, int(src.size())));
    for (const auto &t : b) {
      checked(check);
      for (int da = -1; da <= 1; ++da)
        for (int db = -1; db <= 1; ++db) {
          const auto [first, last] = lookup.equal_range(
              (int(t.a * 500) + da) * 501 + int(t.b * 500) + db);
          for (auto it = first; it != last; ++it) {
            cv::Point2f from[3], to[3];
            for (int k = 0; k < 3; ++k) {
              const auto p = src[t.vertices[k]],
                         q = ref[it->second->vertices[k]];
              from[k] = {float(p.x), float(p.y)};
              to[k] = {float(q.x), float(q.y)};
            }
            // Three corresponding vertices preserve both normal and mirrored
            // fields.
            const auto affine = cv::getAffineTransform(from, to);
            Matrix m;
            std::copy_n(affine.ptr<double>(), 6, m.begin());
            if (!plausible(m))
              continue;
            const int count = int(matcher.match(bright, m, 5).size());
            if (count > bestCount) {
              bestCount = count;
              best = m;
            }
            if (++attempts >= 40000 ||
                bestCount >= std::max(25, int(bright.size() * .65)))
              goto found;
          }
        }
    }
  }
found:
  checked(check);
  if (bestCount < 8)
    throw std::runtime_error("No reliable WCS found. Verify centre, scale, "
                             "overlap and stellar quality.");
  std::vector<std::pair<int, int>> pairs;
  for (int pass = 0; pass < 3; ++pass) {
    pairs = matcher.match(src, best, pass ? 3 : 5);
    if (pairs.size() < 8)
      throw std::runtime_error("Too few consistent astrometric matches.");
    std::vector<cv::Point2d> from, to;
    for (auto [r, s] : pairs) {
      from.emplace_back(src[s].x, src[s].y);
      to.emplace_back(ref[r].x, ref[r].y);
    }
    cv::Mat inliers;
    const auto affine = cv::estimateAffine2D(from, to, inliers, cv::RANSAC, 2.5,
                                             4000, .999, 20);
    if (affine.empty())
      throw std::runtime_error("WCS refinement failed.");
    std::copy_n(affine.ptr<double>(), 6, best.begin());
    checked(check);
  }
  if (!plausible(best))
    throw std::runtime_error("The solved sampling or distortion is "
                             "inconsistent with the input scale.");
  AstrometricSolution result;
  result.wcs = seed;
  // world tangent = seed.CD × (affine × source - seed.CRPIX + 1).
  result.wcs.cd = {seed.cd[0] * best[0] + seed.cd[1] * best[3],
                   seed.cd[0] * best[1] + seed.cd[1] * best[4],
                   seed.cd[2] * best[0] + seed.cd[3] * best[3],
                   seed.cd[2] * best[1] + seed.cd[3] * best[4]};
  const double det = best[0] * best[4] - best[1] * best[3],
               x = seed.crpix[0] - 1 - best[2], y = seed.crpix[1] - 1 - best[5];
  result.wcs.crpix = {1 + (best[4] * x - best[1] * y) / det,
                      1 + (-best[3] * x + best[0] * y) / det};
  const auto centre =
      *result.wcs.sky((image.width - 1) / 2., (image.height - 1) / 2.);
  constexpr double rad = 3.14159265358979323846 / 180.;
  const double separation =
      std::acos(std::clamp(
          std::sin(centre[1] * rad) * std::sin(seed.crval[1] * rad) +
              std::cos(centre[1] * rad) * std::cos(seed.crval[1] * rad) *
                  std::cos((centre[0] - seed.crval[0]) * rad),
          -1., 1.)) /
      rad;
  if (separation >
      std::hypot(image.width, image.height) * seed.scale() / 3600 * 1.25)
    throw std::runtime_error("The solved field lies outside the seeded search "
                             "region. Verify the target coordinates.");
  double minx = image.width, miny = image.height, maxx = 0, maxy = 0, error = 0;
  for (auto [r, s] : matcher.match(src, best, 3)) {
    const auto p =
        result.wcs.pixel(catalog[ref[r].index].ra, catalog[ref[r].index].dec);
    if (!p)
      continue;
    const double residual = std::hypot((*p)[0] - src[s].x, (*p)[1] - src[s].y);
    if (residual > 2.5)
      continue;
    result.matches.push_back(
        {src[s].index, ref[r].index, src[s].x, src[s].y, residual});
    error += residual * residual;
    minx = std::min(minx, src[s].x);
    maxx = std::max(maxx, src[s].x);
    miny = std::min(miny, src[s].y);
    maxy = std::max(maxy, src[s].y);
  }
  result.coverage =
      (maxx - minx) * (maxy - miny) / (double(image.width) * image.height);
  result.rms =
      std::sqrt(error / std::max(std::size_t(1), result.matches.size()));
  if (result.matches.size() < 8 || result.coverage < .15 || result.rms > 2)
    throw std::runtime_error(
        "WCS verification failed: insufficient field coverage or excessive "
        "residuals. A distorted field may require a SIP solver.");
  return result;
}
std::array<double, 3> spectralFlux(const Spectrum &spectrum,
                                   const SpectralResponse &response) {
  if (spectrum.size() < 3 || response.size() < 3 || spectrum.size() > 10000 ||
      response.size() > 2000)
    throw std::runtime_error("Invalid spectrum or RGB response curve.");
  double previous = 0;
  for (const auto &p : spectrum) {
    if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || p[0] <= previous)
      throw std::runtime_error(
          "Spectrum wavelengths must increase and samples must be finite.");
    previous = p[0];
  }
  previous = 0;
  bool nonzero = false;
  for (const auto &p : response) {
    if (!std::isfinite(p.wavelength) || p.wavelength <= previous ||
        p.wavelength < 336 || p.wavelength > 1020)
      throw std::runtime_error("Response wavelengths must increase within Gaia "
                               "XP coverage: 336–1020 nm.");
    previous = p.wavelength;
    for (double v : p.response) {
      if (!std::isfinite(v) || v < 0 || v > 1)
        throw std::runtime_error(
            "Photon response values must be between zero and one.");
      nonzero |= v > 0;
    }
  }
  if (!nonzero || response.front().wavelength < spectrum.front()[0] ||
      response.back().wavelength > spectrum.back()[0])
    throw std::runtime_error(
        "The spectrum must cover the entire RGB response.");
  // A response is zero outside its supplied range. Nonzero endpoints would
  // silently truncate a passband and invalidate the synthetic photometry.
  for (int c = 0; c < 3; ++c)
    if (response.front().response[c] != 0 || response.back().response[c] != 0)
      throw std::runtime_error(
          "RGB response endpoints must be zero; provide complete passbands.");
  std::array<double, 3> sum{};
  // Integrate on the union of both wavelength grids, so narrow structure and
  // filter boundaries survive irregular sampling. F_lambda × lambda converts
  // energy flux to photon counts; the common hc factor cancels in ratios.
  std::vector<double> grid;
  for (const auto &p : response)
    grid.push_back(p.wavelength);
  for (const auto &p : spectrum)
    if (p[0] > response.front().wavelength && p[0] < response.back().wavelength)
      grid.push_back(p[0]);
  std::sort(grid.begin(), grid.end());
  grid.erase(std::unique(grid.begin(), grid.end()), grid.end());
  std::array<Spectrum, 3> curves;
  for (const auto &p : response)
    for (int c = 0; c < 3; ++c)
      curves[c].push_back({p.wavelength, p.response[c]});
  for (std::size_t i = 1; i < grid.size(); ++i) {
    const double a = grid[i - 1], b = grid[i], mid = (a + b) / 2;
    for (int c = 0; c < 3; ++c) {
      auto f = [&](double w) {
        return interpolate(spectrum, w) * w * interpolate(curves[c], w);
      };
      sum[c] += (b - a) * (f(a) + 4 * f(mid) + f(b)) / 6;
    }
  }
  for (double v : sum)
    if (!std::isfinite(v) || v <= 0)
      throw std::runtime_error(
          "Synthetic RGB flux is non-positive. Verify spectrum and passbands.");
  return sum;
}
Spectrum temperatureSpectrum(double kelvin) {
  if (!std::isfinite(kelvin) || kelvin < 2500 || kelvin > 50000)
    throw std::runtime_error(
        "PCC requires a stellar temperature between 2500 and 50000 K.");
  Spectrum s;
  for (int w = 336; w <= 1020; w += 2) {
    const double m = w * 1e-9;
    const double flux =
        1 / (std::pow(m / 550e-9, 5) * std::expm1(.01438776877 / (m * kelvin)));
    s.push_back({double(w), flux});
  }
  return s;
}
std::array<double, 3> referenceColors(const Spectrum &spectrum,
                                      const SpectralResponse &response,
                                      const Spectrum &white) {
  auto flux = spectralFlux(spectrum, response);
  const auto neutral = spectralFlux(white, response);
  for (int c = 0; c < 3; ++c)
    flux[c] /= neutral[c];
  const double green = flux[1];
  for (auto &v : flux)
    v /= green;
  return flux;
}
} // namespace astro::processing
