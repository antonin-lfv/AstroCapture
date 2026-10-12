#include "ObservationWeather.hpp"
#include "Check.hpp"
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <cmath>
#include <stdexcept>
using namespace astro::app;
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  QFile file(ASTRO_SOURCE_DIR "/tests/unit/app/observation-weather.json");
  ASTRO_TEST_CHECK(file.open(QIODevice::ReadOnly));
  const auto bytes = file.readAll();
  const auto forecast =
      parseObservationWeather(bytes, 48.8566, 2.3522, 1791799200);
  ASTRO_TEST_CHECK(observationWeatherMatches(forecast, 48.8566, 2.3522));
  ASTRO_TEST_CHECK(!observationWeatherMatches(forecast, -33.8688, 151.2093));
  const qint64 hour = 1791799200;
  auto value = observationWeatherAt(forecast, hour);
  ASTRO_TEST_CHECK(value["available"].toBool());
  ASTRO_TEST_CHECK(value["cloud_cover"].toDouble() == 60);
  ASTRO_TEST_CHECK(value["cloud_cover_low"].toDouble() == 0);
  ASTRO_TEST_CHECK(value["cloud_cover_mid"].toDouble() == 47);
  ASTRO_TEST_CHECK(value["cloud_cover_high"].toDouble() == 32);
  ASTRO_TEST_CHECK(
      observationWeatherAt(forecast, hour + 1799)["utc"].toLongLong() == hour);
  ASTRO_TEST_CHECK(
      observationWeatherAt(forecast, hour + 1801)["utc"].toLongLong() ==
      hour + 3600);
  const auto series = forecast["samples"].toList();
  ASTRO_TEST_CHECK(observationWeatherAt(
                       forecast, series.first().toMap()["utc"].toLongLong() - 1)
                       .isEmpty());
  ASTRO_TEST_CHECK(observationWeatherAt(
                       forecast, series.last().toMap()["utc"].toLongLong() + 1)
                       .isEmpty());
  auto range = observationWeatherRange(
      forecast,
      {QVariantMap{{"startUtcSeconds", hour}, {"endUtcSeconds", hour + 3600}}});
  ASTRO_TEST_CHECK(range["available"].toBool() && range["hours"].toInt() == 1);
  ASTRO_TEST_CHECK(range["minimum"].toDouble() == 60 &&
                   range["maximum"].toDouble() == 60);
  const auto query = QUrlQuery(observationWeatherUrl(-33.8688, 151.2093));
  ASTRO_TEST_CHECK(query.queryItemValue("timeformat") == "unixtime" &&
                   query.queryItemValue("timezone") == "GMT");
  ASTRO_TEST_CHECK(query.queryItemValue("forecast_days") == "16");
  ASTRO_TEST_CHECK(!query.hasQueryItem("name") && !query.hasQueryItem("date"));
  auto original = QJsonDocument::fromJson(bytes).object();
  auto hourly = original["hourly"].toObject();
  auto cloud = hourly["cloud_cover"].toArray();
  cloud[10] = QJsonValue();
  hourly["cloud_cover"] = cloud;
  auto missing = original;
  missing["hourly"] = hourly;
  auto nullForecast = parseObservationWeather(QJsonDocument(missing).toJson(),
                                              48.8566, 2.3522, hour);
  ASTRO_TEST_CHECK(
      !observationWeatherAt(nullForecast, hour)["available"].toBool());
  ASTRO_TEST_CHECK(!observationWeatherRange(
                        nullForecast, {QVariantMap{{"startUtcSeconds", hour},
                                                   {"endUtcSeconds",
                                                    hour + 3600}}})["available"]
                        .toBool());
  cloud[10] = 0;
  hourly["cloud_cover"] = cloud;
  missing["hourly"] = hourly;
  auto clear = parseObservationWeather(QJsonDocument(missing).toJson(), 48.8566,
                                       2.3522, hour);
  ASTRO_TEST_CHECK(observationWeatherAt(clear, hour)["available"].toBool());
  ASTRO_TEST_CHECK(
      observationWeatherAt(clear, hour)["cloud_cover"].toDouble() == 0);
  auto rejects = [&](QJsonObject bad) {
    bool rejected = false;
    try {
      parseObservationWeather(QJsonDocument(bad).toJson(), 48.8566, 2.3522,
                              hour);
    } catch (const std::exception &) {
      rejected = true;
    }
    ASTRO_TEST_CHECK(rejected);
  };
  cloud[10] = 101;
  hourly["cloud_cover"] = cloud;
  missing["hourly"] = hourly;
  rejects(missing);
  cloud[10] = "clear";
  hourly["cloud_cover"] = cloud;
  missing["hourly"] = hourly;
  rejects(missing);
  auto times = hourly["time"].toArray();
  times[10] = times[9];
  hourly["time"] = times;
  missing["hourly"] = hourly;
  rejects(missing);
  missing = original;
  auto units = missing["hourly_units"].toObject();
  units["cloud_cover"] = "fraction";
  missing["hourly_units"] = units;
  rejects(missing);
  missing = original;
  missing["latitude"] = -33.8;
  rejects(missing);
  missing = original;
  missing["error"] = true;
  rejects(missing);
  missing = original;
  missing["utc_offset_seconds"] = 3600;
  rejects(missing);
  return 0;
}
