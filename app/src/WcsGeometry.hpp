#pragma once
#include "AstrometrySettings.hpp"
#include <QVariantMap>
#include <numbers>
namespace astro::app {
// Match orientImage/transformGeometry pixel centres, including rounded resize.
inline recording::CelestialWcs finishWcs(recording::CelestialWcs w, int width,
                                         int height,
                                         const QVariantMap &settings) {
  auto step = [&](const QVariantMap &s) {
    for (int n = 0; n < s["rotation"].toInt(); ++n) {
      w = w.remap({0, -1, double(height - 1), 1, 0, 0});
      std::swap(width, height);
    }
    if (s["mirrorHorizontal"].toBool())
      w = w.remap({-1, 0, double(width - 1), 0, 1, 0});
    if (s["mirrorVertical"].toBool())
      w = w.remap({1, 0, 0, 0, -1, double(height - 1)});
    const auto g = s["geometry"].toMap();
    if (g["crop"].toBool()) {
      w = w.remap({1, 0, -g["x"].toDouble(), 0, 1, -g["y"].toDouble()});
      width = g["width"].toInt();
      height = g["height"].toInt();
    }
    const double angle = g["angle"].toDouble(),
                 scale = g.value("scale", 1).toDouble();
    if (angle != 0) {
      const double a = angle * std::numbers::pi / 180, c = std::cos(a),
                   sn = std::sin(a);
      const int nw = g["keepSize"].toBool()
                         ? width
                         : int(std::ceil(width * std::abs(c) +
                                         height * std::abs(sn) - 1e-10)),
                nh = g["keepSize"].toBool()
                         ? height
                         : int(std::ceil(width * std::abs(sn) +
                                         height * std::abs(c) - 1e-10));
      const double cx = (width - 1) / 2., cy = (height - 1) / 2.;
      w = w.remap({c, -sn, (nw - 1) / 2. - c * cx + sn * cy, sn, c,
                   (nh - 1) / 2. - sn * cx - c * cy});
      width = nw;
      height = nh;
    }
    if (scale != 1) {
      const int nw = std::max(1, int(std::lround(width * scale))),
                nh = std::max(1, int(std::lround(height * scale)));
      const double sx = double(nw) / width, sy = double(nh) / height;
      w = w.remap({sx, 0, (sx - 1) / 2, 0, sy, (sy - 1) / 2});
      width = nw;
      height = nh;
    }
  };
  for (const auto &value : settings["operations"].toList()) {
    const auto op = value.toMap();
    if (op["enabled"].toBool())
      step(op["settings"].toMap());
  }
  step(settings);
  return w;
}
} // namespace astro::app
