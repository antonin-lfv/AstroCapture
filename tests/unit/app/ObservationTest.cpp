#include "Check.hpp"
#include "ObservationAstronomy.hpp"
#include "ObservationController.hpp"
#include "ObservationPlot.hpp"
#include "astronomy.h"
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <QTimeZone>
#include <QTranslator>
#include <cmath>
#include <iostream>
using namespace astro::app;
static QQuickItem *item(QQuickItem *root, const QString &name) {
  if (root->objectName() == name)
    return root;
  for (auto *child : root->childItems())
    if (auto *found = item(child, name))
      return found;
  return nullptr;
}
int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);
  QTemporaryDir directory;
  ASTRO_TEST_CHECK(directory.isValid());
  QCoreApplication::setOrganizationName("AstroCaptureTest");
  QCoreApplication::setApplicationName("ObservationTest");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                     directory.path());
  QFile references(ASTRO_SOURCE_DIR
                   "/tests/unit/app/observation-horizons.json");
  ASTRO_TEST_CHECK(references.open(QIODevice::ReadOnly));
  for (const auto &v : QJsonDocument::fromJson(references.readAll()).array()) {
    auto r = v.toObject();
    int body = r["horizonsBody"].toInt() == 301   ? BODY_MOON
               : r["horizonsBody"].toInt() == 599 ? BODY_JUPITER
                                                  : BODY_SATURN;
    auto p = observationPosition(
        {{}, {}, {}, body},
        {r["latitude"].toDouble(), r["longitude"].toDouble(), 0},
        QDateTime::fromString(r["utc"].toString(), Qt::ISODate)
            .toSecsSinceEpoch());
    double az = std::abs(
               std::remainder(p.azimuth - r["azimuth"].toDouble(), 360)),
           alt = std::abs(p.altitude - r["altitude"].toDouble());
    std::cout << r["name"].toString().toStdString() << " altitude error "
              << alt * 60 << " arcmin, azimuth error " << az * 60
              << " arcmin\n";
    ASTRO_TEST_CHECK(alt < 1.0 / 60 && az < 1.0 / 60);
  }
  QFile solarReferences(ASTRO_SOURCE_DIR
                        "/tests/unit/app/observation-sun-usno.json");
  ASTRO_TEST_CHECK(solarReferences.open(QIODevice::ReadOnly));
  for (const auto &v :
       QJsonDocument::fromJson(solarReferences.readAll()).array()) {
    const auto reference = v.toObject();
    const auto date =
        QDate::fromString(reference["date"].toString(), Qt::ISODate);
    const auto zone = QTimeZone::fromSecondsAheadOfUtc(
        qRound(reference["offsetHours"].toDouble() * 3600));
    const auto midnight = QDateTime(date, QTime(0, 0), zone).toSecsSinceEpoch();
    const auto calculated =
        observationSolarEvents({reference["latitude"].toDouble(),
                                reference["longitude"].toDouble(), 0},
                               midnight, midnight + 86400);
    for (const auto &entry : reference["events"].toArray()) {
      auto expected = entry.toObject();
      const QString phenomenon = expected["phen"].toString();
      QString kind = phenomenon == "Rise"                   ? "sunrise"
                     : phenomenon == "Set"                  ? "sunset"
                     : phenomenon == "Begin Civil Twilight" ? "civilDawn"
                     : phenomenon == "End Civil Twilight"   ? "civilDusk"
                                                            : "";
      if (kind.isEmpty())
        continue;
      auto actual = std::find_if(calculated.begin(), calculated.end(),
                                 [&](const auto &e) { return e.kind == kind; });
      ASTRO_TEST_CHECK(actual != calculated.end());
      const qint64 expectedUtc =
          QDateTime(date,
                    QTime::fromString(expected["time"].toString(), "HH:mm"),
                    zone)
              .toSecsSinceEpoch();
      ASTRO_TEST_CHECK(std::abs(actual->utc - expectedUtc) < 90);
    }
    if (reference["latitude"].toDouble() > 69) {
      ASTRO_TEST_CHECK(
          std::none_of(calculated.begin(), calculated.end(), [](const auto &e) {
            return e.kind == "sunrise" || e.kind == "sunset";
          }));
    }
  }
  auto spring = observationNoon("2026-03-28", "Europe/Paris"),
       fall = observationNoon("2026-10-24", "Europe/Paris");
  ASTRO_TEST_CHECK(spring.secsTo(QDateTime(spring.date().addDays(1),
                                           QTime(12, 0), spring.timeZone())) ==
                   23 * 3600);
  ASTRO_TEST_CHECK(fall.secsTo(QDateTime(fall.date().addDays(1), QTime(12, 0),
                                         fall.timeZone())) == 25 * 3600);
  auto sydney = observationNoon("2026-10-03", "Australia/Sydney");
  ASTRO_TEST_CHECK(sydney.secsTo(QDateTime(sydney.date().addDays(1),
                                           QTime(12, 0), sydney.timeZone())) ==
                   23 * 3600);
  bool rejected = false;
  try {
    observationNoon("2026-02-31", "Europe/Paris");
  } catch (...) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  rejected = false;
  try {
    observationNoon("2026-10-11", "Invalid/zone");
  } catch (...) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  const qint64 start = observationNoon("2026-10-11", "UTC").toSecsSinceEpoch();
  const ObservationTarget polar{"polar", "polar", "test", -1, 0, 89};
  const ObservationTarget south{"south", "south", "test", -1, 0, -89};
  for (const auto &v :
       observationSamples(polar, {70, 10, 0}, start, start + 86400))
    ASTRO_TEST_CHECK(v.altitude > 0);
  for (const auto &v :
       observationSamples(south, {70, 10, 0}, start, start + 86400))
    ASTRO_TEST_CHECK(v.altitude < 0);
  auto summer = observationNoon("2026-06-21", "UTC").toSecsSinceEpoch();
  ASTRO_TEST_CHECK(
      observationWindows(polar, {89.5, 10, 0}, summer, summer + 86400, 20, -18)
          .empty());
  QTranslator translator;
  ASTRO_TEST_CHECK(translator.load(ASTRO_TRANSLATION_PATH));
  app.installTranslator(&translator);
  QFile weatherFile(ASTRO_SOURCE_DIR
                    "/tests/unit/app/observation-weather.json");
  ASTRO_TEST_CHECK(weatherFile.open(QIODevice::ReadOnly));
  const auto weatherBytes = weatherFile.readAll();
  const auto weatherCache =
      QVariantMap{{"data", weatherBytes},
                  {"latitude", 48.8566},
                  {"longitude", 2.3522},
                  {"receivedUtc", QDateTime::currentSecsSinceEpoch() - 7200}};
  QSettings().setValue("observation/weatherCache", weatherCache);
  ObservationController controller;
  ASTRO_TEST_CHECK(controller.catalogCount() > 13000);
  ASTRO_TEST_CHECK(controller.error().isEmpty());
  auto cloudyNight = controller.settings();
  cloudyNight["date"] = "2026-10-12";
  ASTRO_TEST_CHECK(controller.calculate(cloudyNight));
  controller.setFraction(.5);
  ASTRO_TEST_CHECK(controller.weather()["available"].toBool());
  ASTRO_TEST_CHECK(controller.weather()["cloud_cover"].toInt() == 100);
  ASTRO_TEST_CHECK(controller.weather()["stale"].toBool());
  ASTRO_TEST_CHECK(!controller.weatherSamples().isEmpty());
  controller.setFraction(0);
  ASTRO_TEST_CHECK(controller.weather()["cloud_cover"].toInt() == 60);
  controller.setFraction(.5);
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(!reopened.weather()["busy"].toBool());
    auto options = reopened.settings();
    options["date"] = "2026-10-12";
    ASTRO_TEST_CHECK(reopened.calculate(options));
    ASTRO_TEST_CHECK(reopened.weather()["available"].toBool());
  }
  auto reachablePeriod = controller.settings();
  reachablePeriod["date"] =
      QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
  ASTRO_TEST_CHECK(controller.calculate(reachablePeriod));
  const auto cacheBeforeFailure = QSettings().value("observation/weatherCache");
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
      &controller, "updateReachability", Qt::DirectConnection,
      Q_ARG(int, int(QNetworkInformation::Reachability::Online))));
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
      &controller, "updateReachability", Qt::DirectConnection,
      Q_ARG(int, int(QNetworkInformation::Reachability::Disconnected))));
  controller.refreshWeather();
  ASTRO_TEST_CHECK(controller.weather()["failed"].toBool());
  ASTRO_TEST_CHECK(!controller.weather()["busy"].toBool());
  ASTRO_TEST_CHECK(
      controller.weather()["status"].toString().contains("interrompue"));
  ASTRO_TEST_CHECK(QSettings().value("observation/weatherCache") ==
                   cacheBeforeFailure);
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
      &controller, "updateReachability", Qt::DirectConnection,
      Q_ARG(int, int(QNetworkInformation::Reachability::Online))));
  controller.search("M42");
  ASTRO_TEST_CHECK(
      controller.searchResults().first().toMap()["id"].toString() == "NGC1976");
  controller.search("NGC 7000");
  ASTRO_TEST_CHECK(!controller.searchResults().isEmpty());
  controller.search("M45");
  ASTRO_TEST_CHECK(!controller.searchResults().isEmpty());
  controller.search("Lune");
  ASTRO_TEST_CHECK(
      controller.searchResults().first().toMap()["id"].toString() == "moon");
  auto opts = controller.settings();
  opts["date"] = "2026-12-12";
  opts["minimum"] = 30;
  opts["sunLimit"] = -18;
  ASTRO_TEST_CHECK(controller.calculate(opts));
  for (const auto &v : controller.result()["slots"].toList()) {
    auto w = v.toMap();
    const auto noon =
        observationNoon("2026-12-12", "Europe/Paris").toSecsSinceEpoch();
    const auto begin = noon + qRound64(w["startFraction"].toDouble() * 86400),
               end = noon + qRound64(w["endFraction"].toDouble() * 86400);
    ObservationTarget m42{"m42", "m42",         "test",
                          -1,    83.8186666667, -5.3896666667};
    auto at = observationPosition(m42, {48.8566, 2.3522, 0}, (begin + end) / 2);
    ASTRO_TEST_CHECK(
        at.altitude >= 30 &&
        observationSunAltitude({48.8566, 2.3522, 0}, (begin + end) / 2) <= -18);
  }
  ASTRO_TEST_CHECK(!controller.recommendations().isEmpty());
  const auto winterSuggestions = controller.recommendations();
  ASTRO_TEST_CHECK(controller.result()["recommendationPool"].toInt() > 100);
  for (const auto &value : winterSuggestions) {
    const auto suggestion = value.toMap();
    ObservationTarget target{suggestion["id"].toString(),
                             suggestion["name"].toString(),
                             "test",
                             suggestion["body"].toInt(),
                             suggestion["ra"].toDouble(),
                             suggestion["dec"].toDouble()};
    ASTRO_TEST_CHECK(suggestion["minutes"].toInt() >= 20);
    for (const auto &entry : suggestion["slots"].toList()) {
      const auto w = entry.toMap();
      const auto midpoint = (w["startUtcSeconds"].toLongLong() +
                             w["endUtcSeconds"].toLongLong()) /
                            2;
      ASTRO_TEST_CHECK(
          observationPosition(target, {48.8566, 2.3522, 0}, midpoint)
              .altitude >= 30);
      ASTRO_TEST_CHECK(observationSunAltitude({48.8566, 2.3522, 0}, midpoint) <=
                       suggestion["sunLimit"].toDouble());
    }
  }
  ASTRO_TEST_CHECK(!controller.result()["nightSlots"].toList().isEmpty());
  ASTRO_TEST_CHECK(!controller.result()["daylightSlots"].toList().isEmpty());
  ASTRO_TEST_CHECK(controller.result()["solarEvents"].toList().size() == 6);
  controller.toggleFavoriteTarget();
  ASTRO_TEST_CHECK(controller.targetFavorite());
  ASTRO_TEST_CHECK(controller.favoriteTargets().size() == 1);
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.targetFavorite());
    ASTRO_TEST_CHECK(reopened.selectTarget("NGC1976"));
  }
  controller.toggleFavoriteTarget();
  ASTRO_TEST_CHECK(!controller.targetFavorite());
  auto manual = controller.settings();
  manual["manual"] = true;
  manual["ra"] = 42.125;
  manual["dec"] = -12.25;
  ASTRO_TEST_CHECK(controller.calculate(manual));
  controller.toggleFavoriteTarget();
  const auto manualId = controller.settings()["target"].toString();
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.selectTarget(manualId));
    ASTRO_TEST_CHECK(reopened.targetFavorite());
    ASTRO_TEST_CHECK(reopened.settings()["ra"].toDouble() == 42.125);
    ASTRO_TEST_CHECK(reopened.settings()["dec"].toDouble() == -12.25);
  }
  controller.toggleFavoriteTarget();
  ASTRO_TEST_CHECK(controller.selectTarget("NGC1976"));
  ASTRO_TEST_CHECK(controller.result()["recommendationPool"].toInt() > 100);
  const auto beforeConnectionLoss = controller.result();
  const auto previousSearch = controller.searchResults();
  using Reachability = QNetworkInformation::Reachability;
  auto reachability = [&](Reachability state) {
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
        &controller, "updateReachability", Qt::DirectConnection,
        Q_ARG(int, int(state))));
  };
  reachability(Reachability::Online);
  reachability(Reachability::Unknown);
  reachability(Reachability::Disconnected);
  ASTRO_TEST_CHECK(controller.connectionStatus().contains("Pas de connexion"));
  controller.searchCity("Paris");
  controller.searchOnline("Vega");
  ASTRO_TEST_CHECK(!controller.searching() && !controller.locating());
  ASTRO_TEST_CHECK(controller.searchResults() == previousSearch);
  reachability(Reachability::Online);
  reachability(Reachability::Disconnected);
  ASTRO_TEST_CHECK(controller.connectionStatus().contains("interrompue"));
  ASTRO_TEST_CHECK(controller.result() == beforeConnectionLoss);
  ASTRO_TEST_CHECK(ObservationController::networkFailure(
                       QNetworkReply::TemporaryNetworkFailureError)
                       .contains("interrompue"));
  reachability(Reachability::Online);
  ASTRO_TEST_CHECK(controller.connectionStatus().contains("rétablie"));
  auto old = controller.result();
  opts["latitude"] = 91;
  ASTRO_TEST_CHECK(!controller.calculate(opts));
  ASTRO_TEST_CHECK(controller.result() == old);
  opts["latitude"] = 48.8566;
  ASTRO_TEST_CHECK(controller.calculate(opts));
  const auto utc = controller.instant()["utcSeconds"].toLongLong();
  controller.chooseMapLocation(-33, 151);
  ASTRO_TEST_CHECK(controller.settings()["zone"] == "UTC");
  ASTRO_TEST_CHECK(!controller.weather()["available"].toBool());
  ASTRO_TEST_CHECK(controller.weatherSamples().isEmpty());
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"].toLongLong() == utc);
  ASTRO_TEST_CHECK(controller.recommendations() != winterSuggestions);
  ASTRO_TEST_CHECK(controller.saveSite("Mon jardin"));
  const auto placeCount = controller.sites().size();
  ASTRO_TEST_CHECK(controller.saveSite("Mon jardin renommé"));
  ASTRO_TEST_CHECK(controller.sites().size() == placeCount);
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.sites().last().toMap()["name"] ==
                     "Mon jardin renommé");
  }
  controller.removeSite(controller.sites().size() - 1);
  opts = controller.sites()[0].toMap();
  opts["date"] = "2026-12-12";
  opts["minimum"] = 30;
  opts["sunLimit"] = -18;
  ASTRO_TEST_CHECK(controller.calculate(opts));
  ASTRO_TEST_CHECK(controller.selectTarget("NGC1976"));
  controller.search("");
  auto polarOptions = controller.settings();
  polarOptions["latitude"] = 89.5;
  polarOptions["date"] = "2026-06-21";
  ASTRO_TEST_CHECK(controller.calculate(polarOptions));
  ASTRO_TEST_CHECK(controller.recommendations().isEmpty());
  ASTRO_TEST_CHECK(controller.result()["nightSlots"].toList().isEmpty());
  ASTRO_TEST_CHECK(!controller.result()["polarSun"].toString().isEmpty());
  ASTRO_TEST_CHECK(controller.calculate(opts));
  auto unavailableDate = controller.settings();
  unavailableDate["date"] =
      QDateTime::currentDateTimeUtc().date().addDays(45).toString(Qt::ISODate);
  ASTRO_TEST_CHECK(controller.calculate(unavailableDate));
  controller.refreshWeather();
  ASTRO_TEST_CHECK(!controller.weather()["busy"].toBool());
  ASTRO_TEST_CHECK(!controller.weather()["supported"].toBool());
  ASTRO_TEST_CHECK(controller.calculate(opts));
  qmlRegisterType<ObservationController>("AstroCapture", 1, 0,
                                         "ObservationController");
  qmlRegisterType<ObservationPlot>("AstroCapture", 1, 0, "ObservationPlot");
  QQmlEngine engine;
  QFile file(ASTRO_SOURCE_DIR "/app/qml/ObservationWorkspace.qml");
  ASTRO_TEST_CHECK(file.open(QIODevice::ReadOnly));
  QQmlComponent component(&engine);
  component.setData("import AstroCapture 1.0\n" + file.readAll(),
                    QUrl::fromLocalFile(file.fileName()));
  if (component.isError())
    qWarning() << component.errors();
  ASTRO_TEST_CHECK(component.isReady());
  std::unique_ptr<QObject> object(component.createWithInitialProperties(
      {{"controller", QVariant::fromValue(&controller)}}));
  if (!object)
    qWarning() << component.errors();
  ASTRO_TEST_CHECK(object);
  auto *root = qobject_cast<QQuickItem *>(object.get());
  root->setProperty("viewMode", "night");
  QQuickWindow window;
  window.show();
  root->setParentItem(window.contentItem());
  for (const QSize size :
       {QSize(1440, 960), QSize(1100, 800), QSize(900, 700)}) {
    window.resize(size);
    root->setSize(size);
    QCoreApplication::processEvents();
    QThread::msleep(120);
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(root, "observationPlot")->width() > 400);
    if (!qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS").isEmpty())
      window.grabWindow().save(
          qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS") +
          QString("/night-%1.png").arg(size.width()));
  }
  ASTRO_TEST_CHECK(!controller.targetFavorite());
  auto *favouriteButton = item(root, "observationFavoriteTarget");
  ASTRO_TEST_CHECK(favouriteButton);
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(favouriteButton, "clicked"));
  ASTRO_TEST_CHECK(controller.targetFavorite());
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(favouriteButton, "clicked"));
  ASTRO_TEST_CHECK(!controller.targetFavorite());
  root->setProperty("viewMode", "suggestions");
  const auto firstSuggestion = controller.recommendations().first().toMap();
  auto *explore =
      item(root, "observationSuggestion:" + firstSuggestion["id"].toString());
  ASTRO_TEST_CHECK(explore);
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(explore, "clicked"));
  ASTRO_TEST_CHECK(root->property("viewMode") == "night");
  ASTRO_TEST_CHECK(controller.settings()["target"] == firstSuggestion["id"]);
  ASTRO_TEST_CHECK(controller.settings()["sunLimit"] ==
                   firstSuggestion["sunLimit"]);
  root->setProperty("viewMode", "world");
  QCoreApplication::processEvents();
  if (!qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS").isEmpty())
    window.grabWindow().save(
        qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS") +
        "/world-900.png");
  ASTRO_TEST_CHECK(controller.selectTarget("moon"));
  auto lunar1 = controller.instant()["altitude"].toDouble();
  controller.setFraction(.8);
  ASTRO_TEST_CHECK(
      std::abs(controller.instant()["altitude"].toDouble() - lunar1) > 1);
  ASTRO_TEST_CHECK(controller.settings()["sunLimit"].toInt() == -6);
  root->setProperty("viewMode", "sky");
  QCoreApplication::processEvents();
  if (!qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS").isEmpty())
    window.grabWindow().save(
        qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS") +
        "/moon-horizon-900.png");
  if (qEnvironmentVariableIsSet("ASTRO_OBSERVATION_ONLINE")) {
    auto today = controller.settings();
    today["date"] =
        QDateTime::currentDateTimeUtc().date().toString(Qt::ISODate);
    ASTRO_TEST_CHECK(controller.calculate(today));
    controller.refreshWeather();
    QElapsedTimer weatherTimeout;
    weatherTimeout.start();
    while (controller.weather()["busy"].toBool() &&
           weatherTimeout.elapsed() < 25000) {
      QCoreApplication::processEvents();
      QThread::msleep(10);
    }
    ASTRO_TEST_CHECK(!controller.weather()["busy"].toBool());
    ASTRO_TEST_CHECK(controller.weather()["hasForecast"].toBool());
    ASTRO_TEST_CHECK(controller.weather()["available"].toBool());
    controller.searchCity("Toulouse, France");
    QElapsedTimer timeout;
    timeout.start();
    while (controller.locating() && timeout.elapsed() < 20000) {
      QCoreApplication::processEvents();
      QThread::msleep(10);
    }
    ASTRO_TEST_CHECK(!controller.locating());
    ASTRO_TEST_CHECK(!controller.cityResults().isEmpty());
    ASTRO_TEST_CHECK(controller.selectCity(0));
    ASTRO_TEST_CHECK(controller.settings()["zone"].toString() ==
                     "Europe/Paris");
    controller.searchOnline("Vega");
    timeout.restart();
    while (controller.searching() && timeout.elapsed() < 35000) {
      QCoreApplication::processEvents();
      QThread::msleep(10);
    }
    ASTRO_TEST_CHECK(!controller.searching());
    ASTRO_TEST_CHECK(!controller.searchResults().isEmpty());
    const auto onlineId =
        controller.searchResults().first().toMap()["id"].toString();
    ASTRO_TEST_CHECK(controller.selectTarget(onlineId));
    controller.toggleFavoriteTarget();
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.selectTarget(onlineId));
    ASTRO_TEST_CHECK(reopened.targetFavorite());
  }

  return 0;
}
