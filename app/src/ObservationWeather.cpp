#include "ObservationWeather.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::app {
namespace {
constexpr auto keys = {"cloud_cover", "cloud_cover_low", "cloud_cover_mid",
                       "cloud_cover_high"};
void validateSite(double latitude, double longitude) {
  if (!std::isfinite(latitude) || !std::isfinite(longitude) ||
      std::abs(latitude) > 90 || std::abs(longitude) > 180)
    throw std::runtime_error("Invalid weather location.");
}
void invalid() {
  throw std::runtime_error(
      "Invalid cloud forecast response. Previous forecasts are preserved.");
}
} // namespace
QUrl observationWeatherUrl(double latitude, double longitude) {
  validateSite(latitude, longitude);
  QUrl url("https://api.open-meteo.com/v1/forecast");
  QUrlQuery query;
  query.addQueryItem("latitude", QString::number(latitude, 'g', 17));
  query.addQueryItem("longitude", QString::number(longitude, 'g', 17));
  query.addQueryItem(
      "hourly", "cloud_cover,cloud_cover_low,cloud_cover_mid,cloud_cover_high");
  query.addQueryItem("forecast_days", "16");
  query.addQueryItem("past_days", "1");
  query.addQueryItem("timeformat", "unixtime");
  query.addQueryItem("timezone", "GMT");
  query.addQueryItem("cell_selection", "nearest");
  url.setQuery(query);
  return url;
}
QVariantMap parseObservationWeather(const QByteArray &bytes, double latitude,
                                    double longitude, qint64 receivedUtc) {
  validateSite(latitude, longitude);
  if (bytes.isEmpty() || bytes.size() > 256 * 1024)
    invalid();
  QJsonParseError error;
  auto document = QJsonDocument::fromJson(bytes, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject())
    invalid();
  const auto root = document.object();
  if (root.value("error").toBool() ||
      root.value("utc_offset_seconds").toDouble(NAN) != 0)
    invalid();
  const double gridLat = root.value("latitude").toDouble(NAN),
               gridLon = root.value("longitude").toDouble(NAN);
  if (!std::isfinite(gridLat) || !std::isfinite(gridLon) ||
      std::abs(gridLat) > 90 || std::abs(gridLon) > 180 ||
      std::abs(gridLat - latitude) > 2 ||
      std::abs(std::remainder(gridLon - longitude, 360)) > 2)
    invalid();
  const auto hourly = root.value("hourly").toObject(),
             units = root.value("hourly_units").toObject();
  const auto times = hourly.value("time").toArray();
  if (units.value("time").toString() != "unixtime" || times.isEmpty() ||
      times.size() > 24 * 18)
    invalid();
  for (auto key : keys)
    if (!hourly.value(key).isArray() ||
        hourly.value(key).toArray().size() != times.size() ||
        units.value(key).toString() != "%")
      invalid();
  QVariantList series;
  qint64 previous = 0;
  for (qsizetype i = 0; i < times.size(); ++i) {
    const double raw = times[i].toDouble(NAN);
    if (!std::isfinite(raw) || raw != std::floor(raw) || raw < 0 ||
        raw > 7258118400.0)
      invalid();
    const qint64 utc = qint64(raw);
    if (i > 0 && utc - previous != 3600)
      invalid();
    previous = utc;
    QVariantMap sample{{"utc", utc}};
    for (auto key : keys) {
      auto value = hourly.value(key).toArray()[i];
      if (value.isNull())
        sample[key] = QVariant();
      else {
        const double cloud = value.toDouble(NAN);
        if (!std::isfinite(cloud) || cloud < 0 || cloud > 100)
          invalid();
        sample[key] = cloud;
      }
    }
    sample["available"] = sample["cloud_cover"].isValid();
    series.append(sample);
  }
  return {{"latitude", latitude},       {"longitude", longitude},
          {"gridLatitude", gridLat},    {"gridLongitude", gridLon},
          {"receivedUtc", receivedUtc}, {"samples", series},
          {"source", "Open-Meteo"}};
}
bool observationWeatherMatches(const QVariantMap &forecast, double latitude,
                               double longitude) {
  return !forecast.isEmpty() &&
         forecast.value("latitude", NAN).toDouble() == latitude &&
         forecast.value("longitude", NAN).toDouble() == longitude;
}
QVariantMap observationWeatherAt(const QVariantMap &forecast, qint64 utc) {
  const auto series = forecast.value("samples").toList();
  // Report the closest hourly forecast instant (at most 30 minutes away).
  // No interpolated cloud percentage or extrapolation beyond the data range.
  if (series.isEmpty() ||
      utc < series.first().toMap().value("utc").toLongLong() ||
      utc > series.last().toMap().value("utc").toLongLong())
    return {};
  QVariantMap closest;
  qint64 distance = 1801;
  for (const auto &value : series) {
    auto sample = value.toMap();
    const qint64 delta = std::abs(sample.value("utc").toLongLong() - utc);
    if (delta < distance) {
      closest = sample;
      distance = delta;
    }
  }
  return closest;
}
QVariantMap observationWeatherRange(const QVariantMap &forecast,
                                    const QVariantList &windows) {
  double minimum = 100, maximum = 0;
  int count = 0, missing = 0;
  const auto series = forecast.value("samples").toList();
  // Summarize forecast instants in the windows; this is not a probability of
  // clear sky.
  for (const auto &value : series) {
    auto sample = value.toMap();
    const auto utc = sample.value("utc").toLongLong();
    bool inside = false;
    for (const auto &v : windows) {
      auto w = v.toMap();
      if (utc >= w.value("startUtcSeconds").toLongLong() &&
          utc < w.value("endUtcSeconds").toLongLong()) {
        inside = true;
        break;
      }
    }
    if (!inside)
      continue;
    if (!sample.value("available").toBool()) {
      ++missing;
      continue;
    }
    ++count;
    minimum = std::min(minimum, sample.value("cloud_cover").toDouble());
    maximum = std::max(maximum, sample.value("cloud_cover").toDouble());
  }
  return {{"available", count > 0},
          {"minimum", minimum},
          {"maximum", maximum},
          {"hours", count},
          {"missing", missing}};
}
} // namespace astro::app
