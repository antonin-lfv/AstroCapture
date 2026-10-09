#pragma once
#include "GaiaCatalog.hpp"
#include <QVariantMap>
#include <astro/recording/CelestialWcs.hpp>
namespace astro::app {
inline QVariantList wcsValues(const recording::CelestialWcs &w) {
  return {w.crval[0], w.crval[1], w.crpix[0], w.crpix[1],
          w.cd[0],    w.cd[1],    w.cd[2],    w.cd[3]};
}
inline std::optional<recording::CelestialWcs> readWcs(const QVariant &value) {
  const auto a = value.toList();
  if (a.size() != 8)
    return {};
  double v[8];
  for (int i = 0; i < 8; ++i) {
    bool ok = false;
    v[i] = a[i].toDouble(&ok);
    if (!ok || !std::isfinite(v[i]) || std::abs(v[i]) > 1e9)
      return {};
  }
  recording::CelestialWcs w{
      {v[0], v[1]}, {v[2], v[3]}, {v[4], v[5], v[6], v[7]}};
  if (!w.valid() || w.scale() > 100 || w.scale() < .001)
    return {};
  return w;
}
inline QVariantMap imageAstrometry(const recording::AstroImage &image) {
  QVariantMap out{{"verified", false},
                  {"date", QString::fromStdString(image.date)}};
  if (image.centerRa)
    out["ra"] = *image.centerRa;
  if (image.centerDec)
    out["dec"] = *image.centerDec;
  if (image.pixelScale)
    out["scale"] = *image.pixelScale;
  if (image.wcs) {
    out["wcs"] = wcsValues(*image.wcs);
    out["scale"] = image.wcs->scale();
    if (auto p =
            image.wcs->sky((image.width - 1) / 2., (image.height - 1) / 2.)) {
      out["ra"] = (*p)[0];
      out["dec"] = (*p)[1];
    }
  }
  return out;
}
inline bool validAstrometryState(const QVariantMap &state) {
  try {
    if (state.size() > 30)
      return false;
    if (state.contains("wcs") && !readWcs(state["wcs"]))
      return false;
    readCatalogValues(state["catalog"].toList());
    readCatalogValues(state["localCatalog"].toList());
    for (auto key : {"ra", "dec", "scale", "epoch", "rms", "coverage",
                     "matches", "referenceLimit"})
      if (state.contains(key)) {
        bool ok = false;
        const double v = state[key].toDouble(&ok);
        if (!ok || !std::isfinite(v))
          return false;
        const QString k = key;
        if ((k == "ra" && (v < 0 || v >= 360)) ||
            (k == "dec" && std::abs(v) > 90) ||
            (k == "scale" && (v < .001 || v > 100)) ||
            (k == "epoch" && (v < 1900 || v > 2200)) ||
            (k == "rms" && (v < 0 || v > 2)) ||
            (k == "coverage" && (v < 0 || v > 1.1)) ||
            (k == "referenceLimit" &&
             (v < 8 || v > 150 || v != std::floor(v))) ||
            (k == "matches" && (v < 8 || v > 5000 || v != std::floor(v))))
          return false;
      }
    for (auto key : {"responseName", "responseHash", "localName", "catalogName",
                     "date", "imageKey"})
      if (state.contains(key) && (state[key].typeId() != QMetaType::QString ||
                                  state[key].toString().size() > 512))
        return false;
    const auto positions = state["positions"].toList();
    if (positions.size() > 5000)
      return false;
    for (const auto &point : positions) {
      const auto a = point.toList();
      if (a.size() != 3)
        return false;
      for (int n = 0; n < 3; ++n) {
        bool ok = false;
        const double v = a[n].toDouble(&ok);
        if (!ok || !std::isfinite(v) || v < 0 || v > (n == 2 ? 2.5 : 1))
          return false;
      }
    }
    if (state.contains("response")) {
      const auto rows = state["response"].toList();
      if (rows.size() > 2000)
        return false;
      processing::SpectralResponse response;
      for (const auto &item : rows) {
        const auto a = item.toList();
        if (a.size() != 4)
          return false;
        double v[4];
        for (int n = 0; n < 4; ++n) {
          bool ok = false;
          v[n] = a[n].toDouble(&ok);
          if (!ok || !std::isfinite(v[n]))
            return false;
        }
        response.push_back({v[0], {v[1], v[2], v[3]}});
      }
      processing::spectralFlux(processing::solarSpectrum(), response);
    }
    if (state["verified"].toBool() &&
        (!readWcs(state["wcs"]) || state["catalog"].toList().size() < 8 ||
         state["imageKey"].toString().size() != 64))
      return false;
    return true;
  } catch (const std::exception &) {
    return false;
  }
}
} // namespace astro::app
