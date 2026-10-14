#include "Check.hpp"
#include "ObservationAstronomy.hpp"
#include "ObservationController.hpp"
#include "ObservationPlot.hpp"
#include "astronomy.h"
#include <QElapsedTimer>
#include <QCryptographicHash>
#include <QFile>
#include <QGuiApplication>
#include <QJSValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
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
  return root->findChild<QQuickItem *>(name);
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
  // Preserve the unrefracted solar altitude and apparent upper-limb test
  // when both values are obtained from the same topocentric ephemeris.
  for(const auto date:{"1900-01-01","2026-03-20","2026-06-21","2026-12-21","2100-12-31"})
    for(double latitude:{-89.0,-48.0,0.0,48.0,89.0})
      for(double elevation:{-400.0,0.0,4000.0})
        for(int hour=0;hour<24;++hour) {
          const auto utc=QDateTime(QDate::fromString(date,Qt::ISODate),QTime(hour,0),QTimeZone::UTC).toSecsSinceEpoch();
          const ObservationSite site{latitude,2.35,elevation};
          const auto combined=observationSunPosition(site,utc);
          ASTRO_TEST_CHECK(combined.altitude==observationSunAltitude(site,utc));
          auto time=Astronomy_TimeFromDays((utc-946728000)/86400.0);
          const auto observer=Astronomy_MakeObserver(latitude,2.35,elevation);
          const auto eq=Astronomy_Equator(BODY_SUN,&time,observer,EQUATOR_OF_DATE,ABERRATION);
          const bool expected=Astronomy_Horizon(&time,observer,eq.ra,eq.dec,REFRACTION_NONE).altitude+
              (34.0/60)*Astronomy_Atmosphere(elevation).density+std::asin(0.004650467260962157/eq.dist)*RAD2DEG>=0;
          ASTRO_TEST_CHECK(combined.isUp==expected);
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
  // An ongoing night remains selectable after midnight, but ends at sunrise,
  // rather than at UTC midnight or the following local noon.
  for (const auto &place : std::vector<std::pair<ObservationSite, QString>>{
           {{48.8566, 2.3522, 0}, "Europe/Paris"},
           {{-33.8688, 151.2093, 0}, "Australia/Sydney"},
           {{40.7128, -74.006, 0}, "America/New_York"},
           {{1.87, -157.43, 0}, "Pacific/Kiritimati"}}) {
    const auto ending =
        observationNightEnd(place.first, "2026-10-11", place.second);
    ASTRO_TEST_CHECK(!observationSunIsUp(place.first, ending - 2));
    ASTRO_TEST_CHECK(observationSunIsUp(place.first, ending + 2));
    ASTRO_TEST_CHECK(observationPlanningDate(place.first, place.second,
                                             ending - 1) == "2026-10-11");
    ASTRO_TEST_CHECK(observationPlanningDate(place.first, place.second,
                                             ending) == "2026-10-12");
  }
  for (const auto date : {"2026-03-28", "2026-10-24"}) {
    const auto ending =
        observationNightEnd({48.8566, 2.3522, 0}, date, "Europe/Paris");
    ASTRO_TEST_CHECK(observationPlanningDate({48.8566, 2.3522, 0},
                                             "Europe/Paris",
                                             ending - 1) == date);
    ASTRO_TEST_CHECK(
        observationPlanningDate({48.8566, 2.3522, 0}, "Europe/Paris",
                                ending + 1) ==
        QDate::fromString(date, Qt::ISODate).addDays(1).toString(Qt::ISODate));
  }
  ASTRO_TEST_CHECK(
      observationNightEnd({69.6492, 18.9553, 0}, "2026-12-12", "Europe/Oslo") ==
      observationNoon("2026-12-13", "Europe/Oslo").toSecsSinceEpoch());
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
  if(qEnvironmentVariableIsSet("ASTRO_OBSERVATION_PERF")) {
    QElapsedTimer elapsed;elapsed.start();
    for(int n=0;n<40;++n)controller.setFraction(double(n)/40);
    const auto cursorNs=elapsed.nsecsElapsed();elapsed.restart();
    for(int n=0;n<40;++n)controller.visibleTargets();
    std::cout<<"Observation benchmark: cursor ms/update "<<cursorNs/40.0/1e6
             <<", suggestions ms/read "<<elapsed.nsecsElapsed()/40.0/1e6<<std::endl;
    QCryptographicHash grids(QCryptographicHash::Sha256);
    for(int n=0;n<40;++n){controller.setFraction(double(n)/40);grids.addData(QJsonDocument::fromVariant(controller.world()).toJson(QJsonDocument::Compact));}
    std::cout<<"World grids SHA256 "<<grids.result().toHex().constData()<<std::endl;
    return 0;
  }
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
  const auto targetBeforeSearch = controller.settings()["target"];
  controller.search(" NGC ");
  ASTRO_TEST_CHECK(controller.searchResults().isEmpty());
  ASTRO_TEST_CHECK(controller.searchStatus().contains("numéro"));
  for (const auto *prefix : {"IC", "M", "n"}) {
    controller.search(prefix);
    ASTRO_TEST_CHECK(controller.searchResults().isEmpty());
    ASTRO_TEST_CHECK(!controller.searchStatus().isEmpty());
  }
  controller.search("NGC 1");
  ASTRO_TEST_CHECK(controller.searchResults().size() == 25);
  ASTRO_TEST_CHECK(controller.searchResults().first().toMap()["id"] ==
                   "NGC0001");
  ASTRO_TEST_CHECK(controller.searchStatus().contains("25"));
  controller.search("ngc 5078");
  ASTRO_TEST_CHECK(controller.searchResults().first().toMap()["id"] ==
                   "NGC5078");
  controller.search("M1");
  ASTRO_TEST_CHECK(controller.searchResults().first().toMap()["id"] ==
                   "NGC1952");
  for (const auto &queries : {
           QStringList{"M42", "M 042", "m00042"},
           QStringList{"M45", "m 0045", "M00045"},
           QStringList{"NGC 1", "ngc 00001", "NGC000001"},
           QStringList{"IC 1396", "ic 01396", "IC001396"}}) {
    controller.search(queries.first());
    const auto expected = controller.searchResults();
    ASTRO_TEST_CHECK(!expected.isEmpty());
    for (const auto &query : queries) {
      controller.search(query);
      ASTRO_TEST_CHECK(controller.searchResults() == expected);
    }
  }
  QElapsedTimer searchTimer;
  searchTimer.start();
  for (int i = 0; i < 100; ++i)
    controller.search(i % 2 ? "NGC" : "NGC 1");
  std::cout << "100 broad catalogue searches: " << searchTimer.elapsed()
            << " ms\n";
  ASTRO_TEST_CHECK(searchTimer.elapsed() < 2000);
  ASTRO_TEST_CHECK(controller.settings()["target"] == targetBeforeSearch);
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
  {
    const auto previous = controller.settings();
    auto utah = controller.sites()[9].toMap();
    ASTRO_TEST_CHECK(utah["zone"] == "America/Denver");
    ASTRO_TEST_CHECK(utah["elevation"].toInt() == 1570);
    for (auto it = previous.cbegin(); it != previous.cend(); ++it)
      if (!utah.contains(it.key()))
        utah[it.key()] = it.value();
    utah["latitude"] = "37,7378";
    utah["longitude"] = "-113,6975";
    const auto unchangedUtc = controller.instant()["utcSeconds"];
    ASTRO_TEST_CHECK(controller.changeSite(utah));
    ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == unchangedUtc);
    ASTRO_TEST_CHECK(controller.settings()["latitude"].typeId() ==
                     QMetaType::Double);
    ASTRO_TEST_CHECK(controller.settings()["longitude"].toDouble() ==
                     -113.6975);
    utah = controller.settings();
    utah["date"] = "2026-12-12";
    ASTRO_TEST_CHECK(controller.calculate(utah));
    const auto night = QDateTime(QDate(2026, 12, 13), QTime(0, 30),
                                 QTimeZone("America/Denver"))
                           .toSecsSinceEpoch();
    ASTRO_TEST_CHECK(controller.setReferenceUtc(night));
    controller.toggleFavoriteTarget();
    const auto currentTargets = controller.visibleTargets();
    ASTRO_TEST_CHECK(!currentTargets.isEmpty());
    ASTRO_TEST_CHECK(currentTargets.first().toMap()["id"] == "NGC1976");
    for (const auto &value : currentTargets) {
      const auto candidate = value.toMap();
      const ObservationTarget target{candidate["id"].toString(),
                                     candidate["name"].toString(),
                                     "test",
                                     candidate["body"].toInt(),
                                     candidate["ra"].toDouble(),
                                     candidate["dec"].toDouble()};
      ASTRO_TEST_CHECK(
          observationPosition(target, {37.7378, -113.6975, 1570}, night)
              .altitude >= 30);
      ASTRO_TEST_CHECK(
          observationSunAltitude({37.7378, -113.6975, 1570}, night) <=
          candidate["sunLimit"].toDouble());
    }
    controller.toggleFavoriteTarget();
    const auto afternoon = QDateTime(QDate(2026, 12, 12), QTime(13, 0),
                                     QTimeZone("America/Denver"))
                               .toSecsSinceEpoch();
    ASTRO_TEST_CHECK(controller.setReferenceUtc(afternoon));
    ASTRO_TEST_CHECK(controller.visibleTargets().isEmpty());
    const auto morning = QDateTime(QDate(2026, 12, 13), QTime(10, 0),
                                   QTimeZone("America/Denver"))
                             .toSecsSinceEpoch();
    ASTRO_TEST_CHECK(controller.setReferenceUtc(morning));
    const auto solar = controller.result()["siteSolarEvents"].toList();
    const auto next =
        std::find_if(solar.begin(), solar.end(), [&](const auto &value) {
          const auto event = value.toMap();
          return event["kind"] == "nightStart" &&
                 event["utc"].toLongLong() > morning;
        });
    ASTRO_TEST_CHECK(next != solar.end());
    ASTRO_TEST_CHECK(
        QDateTime::fromSecsSinceEpoch(next->toMap()["utc"].toLongLong(),
                                      QTimeZone("America/Denver"))
            .date() == QDate(2026, 12, 13));
    ASTRO_TEST_CHECK(controller.calculate(previous));
    controller.setFraction(.5);
  }
  {
    const auto savedOptions = controller.settings();
    const auto utcBefore = controller.instant()["utcSeconds"];
    ASTRO_TEST_CHECK(controller.reference()["source"] == "system");
    ASTRO_TEST_CHECK(controller.reference()["zone"] ==
                     QString::fromUtf8(QTimeZone::systemTimeZone().id()));
    ASTRO_TEST_CHECK(controller.setReferenceZone("site"));
    ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == utcBefore);
    ASTRO_TEST_CHECK(controller.settings()["latitude"] ==
                     savedOptions["latitude"]);
    ASTRO_TEST_CHECK(controller.settings()["date"] == savedOptions["date"]);
    ASTRO_TEST_CHECK(!controller.setReferenceZone("invalid"));
    const auto choice =
        controller.referenceTimeChoices("2026-12-13", "00:15")["choices"]
            .toList();
    ASTRO_TEST_CHECK(choice.size() == 1);
    const auto selectedUtc = choice.first().toMap()["utc"].toLongLong();
    ASTRO_TEST_CHECK(selectedUtc == QDateTime(QDate(2026, 12, 13), QTime(0, 15),
                                              QTimeZone("Europe/Paris"))
                                        .toSecsSinceEpoch());
    ASTRO_TEST_CHECK(controller.setReferenceUtc(selectedUtc));
    ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == selectedUtc);
    ASTRO_TEST_CHECK(controller.referenceTimeChoices("2026-12-13", "25:00")
                         .contains("error"));
    ASTRO_TEST_CHECK(controller.referenceTimeChoices("2026-12-14", "00:15")
                         .contains("error"));
    ASTRO_TEST_CHECK(!controller.setReferenceUtc(
        controller.result()["startUtcSeconds"].toLongLong() - 1));
    ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == selectedUtc);
    const auto windows = controller.result()["slots"].toList();
    ASTRO_TEST_CHECK(!windows.isEmpty());
    const auto plan = controller.recordingWindow(
        controller.result()["startUtcSeconds"].toLongLong());
    ASTRO_TEST_CHECK(plan["available"].toBool());
    const auto begin = plan["startUtc"].toLongLong(),
               end = plan["endUtc"].toLongLong();
    ASTRO_TEST_CHECK(begin % 60 == 0 && end % 60 == 0);
    ASTRO_TEST_CHECK(begin >= windows.first().toMap()["startUtc"].toLongLong());
    ASTRO_TEST_CHECK(end <= windows.first().toMap()["endUtc"].toLongLong());
    const ObservationTarget target{"NGC1976",
                                   "M42",
                                   "test",
                                   -1,
                                   savedOptions["ra"].toDouble(),
                                   savedOptions["dec"].toDouble()};
    for (const auto utc : {begin, end}) {
      ASTRO_TEST_CHECK(
          observationPosition(target, {48.8566, 2.3522, 0}, utc).altitude >=
          30);
      ASTRO_TEST_CHECK(observationSunAltitude({48.8566, 2.3522, 0}, utc) <=
                       -18);
    }
    ASTRO_TEST_CHECK(
        !controller
             .recordingWindow(windows.last().toMap()["endUtc"].toLongLong() +
                              1)["available"]
             .toBool());
    auto otherSite = savedOptions;
    const auto sydney = controller.sites()[4].toMap();
    for (auto it = sydney.cbegin(); it != sydney.cend(); ++it)
      otherSite[it.key()] = it.value();
    ASTRO_TEST_CHECK(controller.calculate(otherSite));
    ASTRO_TEST_CHECK(controller.setReferenceZone("system"));
    const auto evening =
        QDateTime(QDate(2026, 12, 12), QTime(18, 0), QTimeZone::UTC)
            .toSecsSinceEpoch();
    ASTRO_TEST_CHECK(controller.setReferenceUtc(evening));
    ASTRO_TEST_CHECK(
        controller.reference()["siteDisplay"] ==
        QDateTime::fromSecsSinceEpoch(evening, QTimeZone("Australia/Sydney"))
            .toString("dd/MM/yyyy HH:mm t"));
    ASTRO_TEST_CHECK(
        controller.reference()["homeDisplay"] ==
        QDateTime::fromSecsSinceEpoch(evening, QTimeZone::systemTimeZone())
            .toString("dd/MM/yyyy HH:mm t"));
    ASTRO_TEST_CHECK(controller.setReferenceZone("site"));
    {
      ObservationController reopened;
      ASTRO_TEST_CHECK(reopened.reference()["source"] == "site");
      ASTRO_TEST_CHECK(reopened.settings()["zone"] == "Australia/Sydney");
    }
    auto spring = savedOptions;
    spring["date"] = "2026-03-28";
    spring["referenceZone"] = "site";
    ASTRO_TEST_CHECK(controller.calculate(spring));
    ASTRO_TEST_CHECK(controller.referenceTimeChoices("2026-03-29", "02:30")
                         .contains("error"));
    ASTRO_TEST_CHECK(
        controller.referenceTimeChoices("2026-03-29", "03:30")["choices"]
            .toList()
            .size() == 1);
    auto autumn = spring;
    autumn["date"] = "2026-10-24";
    ASTRO_TEST_CHECK(controller.calculate(autumn));
    const auto repeated =
        controller.referenceTimeChoices("2026-10-25", "02:30")["choices"]
            .toList();
    ASTRO_TEST_CHECK(repeated.size() == 2);
    ASTRO_TEST_CHECK(repeated[1].toMap()["utc"].toLongLong() -
                         repeated[0].toMap()["utc"].toLongLong() ==
                     3600);
    ASTRO_TEST_CHECK(repeated[0].toMap()["label"] !=
                     repeated[1].toMap()["label"]);
    for (const auto &value : repeated) {
      ASTRO_TEST_CHECK(
          controller.setReferenceUtc(value.toMap()["utc"].toLongLong()));
      ASTRO_TEST_CHECK(controller.reference()["clock"] == "02:30");
    }
    ASTRO_TEST_CHECK(controller.calculate(savedOptions));
    ASTRO_TEST_CHECK(controller.setReferenceZone("system"));
    controller.setFraction(.5);
  }
  {
    const auto paris = controller.settings();
    const auto anchor =
        QDateTime(QDate(2026, 12, 13), QTime(10, 15, 37), QTimeZone::UTC)
            .toSecsSinceEpoch();
    ASTRO_TEST_CHECK(controller.setReferenceUtc(anchor));
    const auto home = controller.reference()["homeDisplay"];
    int updates = 0;
    const auto connection = QObject::connect(
        &controller, &ObservationController::instantChanged, [&] {
          ++updates;
          // Observers must never receive a transient, shifted UTC instant.
          ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == anchor);
          ASTRO_TEST_CHECK(controller.reference()["homeDisplay"] == home);
        });
    auto sydney = paris;
    const auto city = controller.sites()[4].toMap();
    for (auto it = city.cbegin(); it != city.cend(); ++it)
      sydney[it.key()] = it.value();
    ASTRO_TEST_CHECK(controller.changeSite(sydney));
    ASTRO_TEST_CHECK(controller.settings()["date"] == "2026-12-13");
    ASTRO_TEST_CHECK(
        controller.reference()["siteDisplay"] ==
        QDateTime::fromSecsSinceEpoch(anchor, QTimeZone("Australia/Sydney"))
            .toString("dd/MM/yyyy HH:mm t"));
    const auto valid = controller.settings(), result = controller.result(),
               instant = controller.instant();
    auto invalid = valid;
    invalid["latitude"] = 91;
    ASTRO_TEST_CHECK(!controller.changeSite(invalid));
    invalid["latitude"] = valid["latitude"];
    invalid["zone"] = "Invalid/Zone";
    ASTRO_TEST_CHECK(!controller.changeSite(invalid));
    ASTRO_TEST_CHECK(controller.settings() == valid);
    ASTRO_TEST_CHECK(controller.result() == result);
    ASTRO_TEST_CHECK(controller.instant() == instant);
    controller.chooseMapLocation(-33, 151);
    ASTRO_TEST_CHECK(controller.settings()["date"] == "2026-12-12");
    ASTRO_TEST_CHECK(controller.changeSite(paris));
    ASTRO_TEST_CHECK(controller.settings()["date"] == "2026-12-12");
    ASTRO_TEST_CHECK(updates == 3);
    QObject::disconnect(connection);
    // Returning to Paris on a clock-change night also preserves seconds.
    for (const auto date : {QDate(2026, 3, 29), QDate(2026, 10, 25)}) {
      auto dst = paris;
      dst["date"] = date.addDays(-1).toString(Qt::ISODate);
      ASTRO_TEST_CHECK(controller.calculate(dst));
      const auto utc =
          QDateTime(date, QTime(1, 30, 43), QTimeZone::UTC).toSecsSinceEpoch();
      ASTRO_TEST_CHECK(controller.setReferenceUtc(utc));
      ASTRO_TEST_CHECK(controller.changeSite(sydney));
      ASTRO_TEST_CHECK(controller.changeSite(dst));
      ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == utc);
      const auto seconds = controller.result()["endUtcSeconds"].toLongLong() -
                           controller.result()["startUtcSeconds"].toLongLong();
      ASTRO_TEST_CHECK(seconds == (date.month() == 3 ? 23 : 25) * 3600);
    }
    ASTRO_TEST_CHECK(controller.calculate(paris));
    controller.setFraction(.5);
  }
  auto polarOptions = controller.settings();
  polarOptions["latitude"] = 89.5;
  polarOptions["date"] = "2026-06-21";
  ASTRO_TEST_CHECK(controller.calculate(polarOptions));
  ASTRO_TEST_CHECK(controller.recommendations().isEmpty());
  ASTRO_TEST_CHECK(controller.result()["nightSlots"].toList().isEmpty());
  ASTRO_TEST_CHECK(!controller.result()["polarSun"].toString().isEmpty());
  QVariantList daylightBefore;
  for (const auto &entry : controller.world()) {
    const auto cell = entry.toMap();
    daylightBefore.append(cell["solarState"]);
    if (cell["lat"].toInt() == 87)
      ASTRO_TEST_CHECK(cell["solarState"].toInt() == 0);
    if (cell["lat"].toInt() == -87)
      ASTRO_TEST_CHECK(cell["solarState"].toInt() == 2);
  }
  ASTRO_TEST_CHECK(controller.selectTarget("moon"));
  const auto solarWorld = controller.world();
  for (int i = 0; i < solarWorld.size(); ++i)
    ASTRO_TEST_CHECK(solarWorld[i].toMap()["solarState"] == daylightBefore[i]);
  ASTRO_TEST_CHECK(controller.selectTarget("NGC1976"));
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
      {{"controller", QVariant::fromValue(&controller)},
       {"liveNow", false},
       {"autoLoadWeather", false},
       {"autoSelectVisible", false}}));
  if (!object)
    qWarning() << component.errors();
  ASTRO_TEST_CHECK(object);
  auto *root = qobject_cast<QQuickItem *>(object.get());
  ASTRO_TEST_CHECK(!root->property("liveNow").toBool());
  ASTRO_TEST_CHECK(!item(root, "observationInspector"));
  ASTRO_TEST_CHECK(root->property("suggestionPeriod") == "instant");
  ASTRO_TEST_CHECK(item(root, "observationSiteSummary"));
  const auto assertSharedClocks = [&] {
    const auto utc = controller.instant()["utcSeconds"].toLongLong();
    const auto home =
        QDateTime::fromSecsSinceEpoch(utc, QTimeZone::systemTimeZone())
            .toString("dd/MM/yyyy HH:mm");
    const auto site =
        QDateTime::fromSecsSinceEpoch(
            utc, QTimeZone(controller.settings()["zone"].toString().toUtf8()))
            .toString("dd/MM/yyyy HH:mm");
    for (const auto *name :
         {"observationPcClock", "observationSharedPcClock"}) {
      const auto text = item(root, name)->property("text").toString();
      ASTRO_TEST_CHECK(text.endsWith(home));
      ASTRO_TEST_CHECK(!text.contains("GMT"));
    }
    for (const auto *name :
         {"observationSiteClock", "observationSharedSiteClock"})
      ASTRO_TEST_CHECK(
          item(root, name)->property("text").toString().endsWith(site));
  };
  assertSharedClocks();
  for (const auto *name : {"observationPlot", "observationCloudPlot"}) {
    const auto previous = item(root, "observationPcClock")->property("text");
    const auto fraction = QString(name) == "observationPlot" ? .23 : .71;
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
        item(root, name), "fractionChosen", Q_ARG(double, fraction)));
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "finishTimeUpdate"));
    ASTRO_TEST_CHECK(item(root, "observationPcClock")->property("text") !=
                     previous);
    assertSharedClocks();
  }
  root->setProperty("autoSelectVisible", true);
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "updateVisibleTarget"));
  if (!controller.visibleTargets().isEmpty())
    ASTRO_TEST_CHECK(controller.settings()["target"] ==
                     controller.visibleTargets().first().toMap()["id"]);
  root->setProperty("autoSelectVisible", false);
  ASTRO_TEST_CHECK(controller.setReferenceZone("site"));
  const auto referenceDays = controller.reference()["days"].toList();
  int followingDay = -1;
  for (int i = 0; i < referenceDays.size(); ++i)
    if (referenceDays[i].toMap()["date"] == "2026-12-13")
      followingDay = i;
  ASTRO_TEST_CHECK(followingDay >= 0);
  item(root, "observationReferenceDay")
      ->setProperty("currentIndex", followingDay);
  item(root, "observationReferenceClock")->setProperty("text", "00:15");
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "applyReferenceTime"));
  ASTRO_TEST_CHECK(controller.reference()["clock"] == "00:15");
  assertSharedClocks();
  QCoreApplication::processEvents();
  ASTRO_TEST_CHECK(
      item(root, "observationReferenceDay")->property("currentIndex").toInt() ==
      followingDay);
  ASTRO_TEST_CHECK(
      std::abs(item(root, "observationTime")->property("value").toDouble() -
               controller.instant()["fraction"].toDouble()) < 1e-6);
  const auto referenceUtc = controller.instant()["utcSeconds"];
  item(root, "observationReferenceClock")->setProperty("text", "25:00");
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "applyReferenceTime"));
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == referenceUtc);
  ASTRO_TEST_CHECK(!root->property("referenceNotice").toString().isEmpty());
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "showRecordingWindow"));
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] ==
                   controller.reference()["recording"].toMap()["startUtc"]);
  ASTRO_TEST_CHECK(root->property("referenceNotice").toString().isEmpty());
  assertSharedClocks();
  const auto recordingStart =
      controller.reference()["recording"].toMap()["startUtc"].toLongLong();
  const auto recordingClock =
      QDateTime::fromSecsSinceEpoch(recordingStart, QTimeZone::systemTimeZone())
          .toString("dd/MM/yyyy HH:mm");
  ASTRO_TEST_CHECK(item(root, "observationRecordingStart")
                       ->property("text")
                       .toString()
                       .endsWith(recordingClock));
  ASTRO_TEST_CHECK(!item(root, "observationRecordingEnd")
                        ->property("text")
                        .toString()
                        .contains("GMT"));
  controller.setFraction(.512345);
  const auto preciseUtc = controller.instant()["utcSeconds"];
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "applyReferenceTime"));
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == preciseUtc);
  ASTRO_TEST_CHECK(controller.setReferenceZone("system"));
  auto *sitePicker = item(root, "observationSite");
  auto pickSite = [&](int sourceIndex) {
    const auto sorted =
        root->property("sortedSites").value<QJSValue>().toVariant().toList();
    int sortedIndex = -1;
    bool seenOrdinary = false;
    for (int i = 0; i < sorted.size(); ++i) {
      const auto value = sorted[i].toMap();
      if (value["sourceIndex"].toInt() == sourceIndex)
        sortedIndex = i;
      if (!value["favorite"].toBool())
        seenOrdinary = true;
      else
        ASTRO_TEST_CHECK(!seenOrdinary);
    }
    ASTRO_TEST_CHECK(sortedIndex >= 0);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(sitePicker, "activated",
                                               Q_ARG(int, sortedIndex)));
    root->setProperty("autoSelectVisible", false);
  };
  pickSite(4);
  ASTRO_TEST_CHECK(controller.settings()["zone"] == "Australia/Sydney");
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == preciseUtc);
  ASTRO_TEST_CHECK(controller.saveSite("Test remote telescope"));
  const int savedSite = controller.sites().size() - 1;
  pickSite(0);
  pickSite(savedSite);
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == preciseUtc);
  QVariant favoriteIndex;
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
      root, "favoriteSiteIndex", Q_RETURN_ARG(QVariant, favoriteIndex)));
  ASTRO_TEST_CHECK(favoriteIndex.toInt() == savedSite);
  const auto beforeSiteActions = controller.instant()["utcSeconds"];
  auto *placeStar = item(root, "observationFavoritePlace");
  ASTRO_TEST_CHECK(placeStar->property("checked").toBool());
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(placeStar, "clicked"));
  ASTRO_TEST_CHECK(!controller.sites()[savedSite].toMap()["favorite"].toBool());
  ASTRO_TEST_CHECK(controller.sites().size() == savedSite + 1);
  ASTRO_TEST_CHECK(!placeStar->property("checked").toBool());
  ASTRO_TEST_CHECK(controller.saveSite("Lieu renommé sans étoile"));
  ASTRO_TEST_CHECK(!controller.sites()[savedSite].toMap()["favorite"].toBool());
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.sites().last().toMap()["name"] ==
                     "Lieu renommé sans étoile");
    ASTRO_TEST_CHECK(!reopened.sites().last().toMap()["favorite"].toBool());
  }
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(placeStar, "clicked"));
  ASTRO_TEST_CHECK(controller.sites()[savedSite].toMap()["favorite"].toBool());
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
      item(root, "observationDeletePlace"), "clicked"));
  ASTRO_TEST_CHECK(controller.sites().size() == savedSite);
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == beforeSiteActions);
  ASTRO_TEST_CHECK(root->property("selectedSiteIndex").toInt() == -1);
  ASTRO_TEST_CHECK(!item(root, "observationDeletePlace")->isVisible());
  // A personal copy of a preset can retain its original name. Its controls
  // must affect the saved copy, including after it loses its favourite star.
  pickSite(0);
  const auto presetName = controller.settings()["name"].toString();
  const auto websiteUtc = controller.instant()["utcSeconds"];
  ASTRO_TEST_CHECK(controller.saveSite(presetName, "https://example.org/telescope?site=paris"));
  const int copyIndex = controller.sites().size() - 1;
  ASTRO_TEST_CHECK(root->property("selectedSiteIndex").toInt() == copyIndex);
  controller.toggleFavoriteSite(copyIndex);
  ASTRO_TEST_CHECK(root->property("selectedSiteIndex").toInt() == copyIndex);
  ASTRO_TEST_CHECK(root->property("selectedSite").toMap()["website"] ==
                   "https://example.org/telescope?site=paris");
  const auto savedCopy = controller.sites();
  for (const auto *invalid : {"file:///tmp/test", "javascript:alert(1)",
                             "https://user:password@example.org", "https://", "not a URL"}) {
    ASTRO_TEST_CHECK(!controller.saveSite("Invalid", invalid));
    ASTRO_TEST_CHECK(controller.sites() == savedCopy);
    ASTRO_TEST_CHECK(controller.settings()["name"] == presetName);
  }
  ASTRO_TEST_CHECK(controller.saveSite(presetName));
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.sites().last().toMap()["website"] ==
                     "https://example.org/telescope?site=paris");
    ASTRO_TEST_CHECK(!reopened.sites().last().toMap()["favorite"].toBool());
  }
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(root, "observationNamePlace"), "clicked"));
  ASTRO_TEST_CHECK(item(root, "observationPlaceWebsite")->property("text") ==
                   "https://example.org/telescope?site=paris");
  item(root, "observationPlaceWebsite")->setProperty("text", QStringLiteral(""));
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(item(root, "observationSavePlace"), "clicked"));
  ASTRO_TEST_CHECK(controller.sites().last().toMap()["website"].toString().isEmpty());
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == websiteUtc);
  controller.removeSite(copyIndex);
  ASTRO_TEST_CHECK(root->property("selectedSiteIndex").toInt() == 0);
  for (const char *kind : {"sunset", "sunrise"}) {
    for (const auto &value : controller.result()["siteSolarEvents"].toList()) {
      const auto event = value.toMap();
      if (event["kind"] != kind ||
          event["utc"].toLongLong() <=
              controller.instant()["utcSeconds"].toLongLong())
        continue;
      const auto clock =
          QDateTime::fromSecsSinceEpoch(event["utc"].toLongLong())
              .toLocalTime()
              .toString("dd/MM HH:mm");
      ASTRO_TEST_CHECK(
          item(root, QString("observation") +
                         (QString(kind) == "sunset" ? "Sunset" : "Sunrise"))
              ->property("text")
              .toString()
              .contains(clock));
      break;
    }
  }
  // Exact coordinates and a manually selected time zone share the same rule.
  item(root, "observationLatitude")->setProperty("text", "-34.25");
  item(root, "observationElevation")->setProperty("text", "120");
  item(root, "observationZone")->setProperty("text", "Australia/Perth");
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "calculate"));
  ASTRO_TEST_CHECK(controller.settings()["latitude"].toDouble() == -34.25);
  ASTRO_TEST_CHECK(controller.settings()["elevation"].toDouble() == 120);
  ASTRO_TEST_CHECK(controller.settings()["zone"] == "Australia/Perth");
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == preciseUtc);
  pickSite(0);
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == preciseUtc);
  ASTRO_TEST_CHECK(item(root, "observationPlot")->isVisible());
  ASTRO_TEST_CHECK(item(root, "observationWindows")->isVisible());
  ASTRO_TEST_CHECK(item(root, "observationSkyPlot")->isVisible());
  ASTRO_TEST_CHECK(item(root, "observationWorldPlot")->isVisible());
  ASTRO_TEST_CHECK(item(root, "observationCloudCard")->isVisible());
  const auto validSettings = controller.settings();
  auto *dateField = item(root, "observationDate");
  dateField->setProperty(
      "text", QDate::currentDate().addDays(-7).toString(Qt::ISODate));
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "calculate"));
  ASTRO_TEST_CHECK(controller.settings() == validSettings);
  ASTRO_TEST_CHECK(dateField->property("text") == validSettings["date"]);
  ASTRO_TEST_CHECK(!root->property("dateNotice").toString().isEmpty());
  auto *history = item(root, "observationHistory");
  history->setProperty("checked", true);
  dateField->setProperty(
      "text", QDate::currentDate().addDays(-7).toString(Qt::ISODate));
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "calculate"));
  ASTRO_TEST_CHECK(controller.settings()["date"] ==
                   QDate::currentDate().addDays(-7).toString(Qt::ISODate));
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "currentNight"));
  ASTRO_TEST_CHECK(!history->property("checked").toBool());
  ASTRO_TEST_CHECK(
      !controller.planningNight(controller.settings())["past"].toBool());
  ASTRO_TEST_CHECK(root->property("dateNotice").toString().isEmpty());
  auto invalidPlanning = controller.settings();
  invalidPlanning["latitude"] = "not a number";
  ASTRO_TEST_CHECK(controller.planningNight(invalidPlanning).contains("error"));
  ASTRO_TEST_CHECK(controller.calculate(validSettings));
  const auto selectedId = controller.settings()["target"];
  controller.search("moon");
  ASTRO_TEST_CHECK(controller.settings()["target"] == selectedId);
  ASTRO_TEST_CHECK(
      QMetaObject::invokeMethod(root, "chooseTarget", Q_ARG(QVariant, "moon")));
  ASTRO_TEST_CHECK(controller.settings()["target"] == "moon");
  ASTRO_TEST_CHECK(item(root, "observationSelectedTarget")
                       ->property("text")
                       .toString()
                       .endsWith(controller.result()["target"].toString()));
  ASTRO_TEST_CHECK(controller.selectTarget(selectedId.toString()));
  QQuickWindow window;
  window.show();
  root->setParentItem(window.contentItem());
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "openObjectChooser"));
  auto *searchField = item(root, "observationSearch");
  int searches = 0;
  const auto searchConnection = QObject::connect(
      &controller, &ObservationController::searchChanged, [&] { ++searches; });
  for (const auto *text : {"N", "NG", "NGC", "NGC 5", "NGC 50", "NGC 5078"})
    searchField->setProperty("text", text);
  ASTRO_TEST_CHECK(searches == 0);
  QElapsedTimer debounceTimer;
  debounceTimer.start();
  while (searches == 0 && debounceTimer.elapsed() < 1000) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
  ASTRO_TEST_CHECK(searches == 1);
  ASTRO_TEST_CHECK(controller.searchResults().first().toMap()["id"] ==
                   "NGC5078");
  QObject::disconnect(searchConnection);
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
      root->findChild<QObject *>("observationObjectChooser"), "close"));
  for (const QSize size :
       {QSize(1440, 960), QSize(1100, 800), QSize(900, 700)}) {
    window.resize(size);
    root->setSize(size);
    QCoreApplication::processEvents();
    QThread::msleep(120);
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(item(root, "observationPlot")->width() > 320);
    const auto placeWidth = item(root, "observationSiteSummary")->width();
    const auto mapWidth = item(root, "observationDayCard")->width();
    ASTRO_TEST_CHECK(std::abs(mapWidth / placeWidth - 3) < .02);
    const auto site = item(root, "observationSiteSummary");
    const auto time = item(root, "observationTimeCard");
    const auto sky = item(root, "observationSkyCard");
    ASTRO_TEST_CHECK(std::abs(site->width() - time->width()) < 1);
    ASTRO_TEST_CHECK(std::abs(sky->width() - time->width()) < 1);
    ASTRO_TEST_CHECK(std::abs(sky->mapToItem(root, QPointF()).x() -
                              time->mapToItem(root, QPointF()).x()) < 1);
    ASTRO_TEST_CHECK(sky->mapToItem(root, QPointF()).y() >=
                     time->mapToItem(root, QPointF()).y() + time->height() +
                         15);
    ASTRO_TEST_CHECK(std::abs(item(root, "observationWorldCard")->width() -
                              item(root, "observationDayCard")->width()) < 1);
    ASTRO_TEST_CHECK(time->mapToItem(root, QPointF()).y() >=
                     site->mapToItem(root, QPointF()).y() + site->height() +
                         15);
    ASTRO_TEST_CHECK(
        std::abs(item(root, "observationSuggestionPeriod")->height() -
                 item(root, "observationVisibleTarget")->height()) < 1);
    ASTRO_TEST_CHECK(std::abs(item(root, "observationHeightCard")->width() -
                              item(root, "observationCloudCard")->width()) < 1);
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
  root->setProperty("suggestionPeriod", "night");
  ASTRO_TEST_CHECK(!item(root, "observationSuggestions"));
  ASTRO_TEST_CHECK(!item(root, "observationDetails"));
  const auto firstSuggestion = controller.recommendations().first().toMap();
  auto *recommended = item(root, "observationVisibleTarget");
  ASTRO_TEST_CHECK(recommended);
  ASTRO_TEST_CHECK(
      QMetaObject::invokeMethod(recommended, "activated", Q_ARG(int, 0)));
  ASTRO_TEST_CHECK(controller.settings()["target"] == firstSuggestion["id"]);
  ASTRO_TEST_CHECK(controller.settings()["sunLimit"] ==
                   firstSuggestion["sunLimit"]);
  QCoreApplication::processEvents();
  const auto mapUtc = controller.instant()["utcSeconds"];
  ASTRO_TEST_CHECK(item(root, "observationDayPlot")->property("mode") ==
                   "daylight");
  ASTRO_TEST_CHECK(item(root, "observationWorldPlot")->property("mode") ==
                   "world");
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == mapUtc);
  ASTRO_TEST_CHECK(item(root, "observationWindows")->isVisible());
  // Keep the mouse down and move continuously: the chart's instant must update
  // before release, rather than waiting for a quiet interval after dragging.
  auto *scroll = item(root, "observationScroll")
                     ->property("contentItem")
                     .value<QQuickItem *>();
  auto *timeCard = item(root, "observationTimeCard");
  QCoreApplication::processEvents();
  scroll->setProperty("contentY",
                      scroll->property("contentY").toDouble() +
                          timeCard->mapToItem(scroll, QPointF()).y() - 16);
  QCoreApplication::processEvents();
  auto *slider = item(root, "observationTime");
  auto mouse = [&](QEvent::Type type, QPointF position, Qt::MouseButton button,
                   Qt::MouseButtons buttons) {
    QMouseEvent event(type, position, window.mapToGlobal(position.toPoint()),
                      button, buttons, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &event);
  };
  {
    slider->setProperty("value", .2);
    controller.setFraction(.2);
    QCoreApplication::processEvents();
    auto *handle = slider->property("handle").value<QQuickItem *>();
    ASTRO_TEST_CHECK(handle);
    const auto position =
        handle->mapToScene(QPointF(handle->width() / 2, handle->height() / 2));
    int updatesDuringDrag = 0;
    root->setProperty("autoSelectVisible", true);
    auto connection = QObject::connect(
        &controller, &ObservationController::instantChanged, [&] {
          if (slider->property("pressed").toBool())
            ++updatesDuringDrag;
        });
    mouse(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
    ASTRO_TEST_CHECK(slider->property("pressed").toBool());
    QPointF last;
    for (int i = 1; i <= 40; ++i) {
      last = position + QPointF(slider->width() * .6 * i / 40, 0);
      mouse(QEvent::MouseMove, last, Qt::NoButton, Qt::LeftButton);
      QCoreApplication::processEvents();
      QThread::msleep(6);
      assertSharedClocks();
    }
    ASTRO_TEST_CHECK(slider->property("pressed").toBool());
    ASTRO_TEST_CHECK(updatesDuringDrag >= 3);
    ASTRO_TEST_CHECK(controller.instant()["fraction"].toDouble() > .5);
    mouse(QEvent::MouseButtonRelease, last, Qt::LeftButton, Qt::NoButton);
    ASTRO_TEST_CHECK(!slider->property("pressed").toBool());
    ASTRO_TEST_CHECK(std::abs(controller.instant()["fraction"].toDouble() -
                              slider->property("value").toDouble()) < 1e-6);
    QObject::disconnect(connection);
    root->setProperty("autoSelectVisible", false);
  }
  if (!qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS").isEmpty())
    window.grabWindow().save(
        qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS") +
        "/world-900.png");
  // The conversion remains visible when the top cards scroll out of view.
  scroll->setProperty(
      "contentY",
      scroll->property("contentY").toDouble() +
          item(root, "observationCurvesRow")->mapToItem(scroll, QPointF()).y());
  QCoreApplication::processEvents();
  auto *sharedClock = item(root, "observationSharedPcClock");
  ASTRO_TEST_CHECK(sharedClock->isVisible() &&
                   sharedClock->mapToScene(QPointF()).y() >= 0 &&
                   sharedClock->mapToScene(QPointF()).y() < 120);
  assertSharedClocks();
  const auto savedSelection = controller.settings();
  const auto savedTarget = controller.settings()["target"];
  auto emptySelection = savedSelection;
  emptySelection["minimum"] = 90;
  ASTRO_TEST_CHECK(controller.calculate(emptySelection));
  ASTRO_TEST_CHECK(controller.visibleTargets().isEmpty());
  ASTRO_TEST_CHECK(controller.recommendations().isEmpty());
  auto *targetMenu = item(root, "observationVisibleTarget");
  for (const auto *period : {"instant", "night"}) {
    root->setProperty("suggestionPeriod", period);
    ASTRO_TEST_CHECK(root->property("filteredSuggestions")
                         .value<QJSValue>()
                         .toVariant()
                         .toList()
                         .isEmpty());
    ASTRO_TEST_CHECK(!targetMenu->property("enabled").toBool());
    const auto expected = QCoreApplication::translate(
        "ObservationWorkspace", QString(period) == "instant"
                                    ? "Nothing visible at this time"
                                    : "Nothing visible during this night");
    ASTRO_TEST_CHECK(targetMenu->property("displayText").toString() ==
                     expected);
    ASTRO_TEST_CHECK(controller.settings()["target"] == savedTarget);
  }
  ASTRO_TEST_CHECK(controller.calculate(savedSelection));
  root->setProperty("suggestionPeriod", "instant");
  assertSharedClocks();
  const auto locationUtc = controller.instant()["utcSeconds"];
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(
      item(root, "observationWorldPlot"), "locationChosen", Q_ARG(double, -33),
      Q_ARG(double, 151)));
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == locationUtc);
  assertSharedClocks();
  root->setProperty("autoSelectVisible", false);
  reachability(Reachability::Disconnected);
  controller.resolveMapTimeZone();
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(&controller, "fetchMapTimeZone",
                                             Qt::DirectConnection));
  ASTRO_TEST_CHECK(!controller.mapTimeZoneBusy());
  ASTRO_TEST_CHECK(!controller.mapTimeZoneStatus().isEmpty());
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == locationUtc);
  const auto futureSettings = controller.settings();
  reachability(Reachability::Disconnected);
  const auto beforeNow = QDateTime::currentSecsSinceEpoch();
  ASTRO_TEST_CHECK(
      QMetaObject::invokeMethod(item(root, "observationNow"), "clicked"));
  const auto actualNow = controller.instant()["utcSeconds"].toLongLong();
  ASTRO_TEST_CHECK(actualNow >= beforeNow &&
                   actualNow <= QDateTime::currentSecsSinceEpoch());
  ASTRO_TEST_CHECK(root->property("liveNow").toBool());
  ASTRO_TEST_CHECK(root->findChild<QObject *>("observationLiveClock")
                       ->property("running")
                       .toBool());
  ASTRO_TEST_CHECK(controller.settings()["latitude"] ==
                   futureSettings["latitude"]);
  ASTRO_TEST_CHECK(controller.settings()["zone"] == futureSettings["zone"]);
  ASTRO_TEST_CHECK(controller.result()["startUtcSeconds"].toLongLong() <=
                   actualNow);
  ASTRO_TEST_CHECK(controller.result()["endUtcSeconds"].toLongLong() >
                   actualNow);
  item(root, "observationReferenceClock")->setProperty("text", "25:00");
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "applyReferenceTime"));
  ASTRO_TEST_CHECK(!root->property("liveNow").toBool());
  ASTRO_TEST_CHECK(!root->property("referenceNotice").toString().isEmpty());
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == actualNow);
  ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "goNow"));
  ASTRO_TEST_CHECK(root->property("liveNow").toBool());
  ASTRO_TEST_CHECK(root->property("referenceNotice").toString().isEmpty());
  ASTRO_TEST_CHECK(
      QMetaObject::invokeMethod(root, "chooseFraction", Q_ARG(QVariant, .4)));
  ASTRO_TEST_CHECK(!root->property("liveNow").toBool());
  ASTRO_TEST_CHECK(!root->findChild<QObject *>("observationLiveClock")
                        ->property("running")
                        .toBool());
  {
    auto utah = controller.settings();
    const auto preset = controller.sites()[9].toMap();
    for (auto it = preset.cbegin(); it != preset.cend(); ++it)
      utah[it.key()] = it.value();
    ASTRO_TEST_CHECK(controller.changeSite(utah));
    ASTRO_TEST_CHECK(controller.showNow());
    const auto actual = controller.instant()["utcSeconds"].toLongLong();
    const bool dark = controller.instant()["sun"].toDouble() <= -18;
    qint64 upcoming = 0;
    for (const auto &value : controller.result()["siteSolarEvents"].toList()) {
      const auto event = value.toMap();
      if (event["kind"] == "nightStart" && event["utc"].toLongLong() > actual) {
        upcoming = event["utc"].toLongLong();
        break;
      }
    }
    ASTRO_TEST_CHECK(upcoming > 0);
    ASTRO_TEST_CHECK(QMetaObject::invokeMethod(root, "prepareNight"));
    QThread::msleep(50);
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(!root->property("liveNow").toBool());
    ASTRO_TEST_CHECK(root->property("suggestionPeriod") == "night");
    ASTRO_TEST_CHECK(std::abs(controller.instant()["utcSeconds"].toLongLong() -
                              (dark ? actual : upcoming)) <= 2);
  }
  ASTRO_TEST_CHECK(controller.calculate(futureSettings));
  ASTRO_TEST_CHECK(controller.selectTarget("moon"));
  auto lunar1 = controller.instant()["altitude"].toDouble();
  controller.setFraction(.8);
  ASTRO_TEST_CHECK(
      std::abs(controller.instant()["altitude"].toDouble() - lunar1) > 1);
  ASTRO_TEST_CHECK(controller.settings()["sunLimit"].toInt() == -6);
  QCoreApplication::processEvents();
  if (!qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS").isEmpty())
    window.grabWindow().save(
        qEnvironmentVariable("ASTRO_OBSERVATION_SCREENSHOTS") +
        "/moon-horizon-900.png");
  if (qEnvironmentVariableIsSet("ASTRO_OBSERVATION_ONLINE")) {
    reachability(Reachability::Online);
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

  if (qEnvironmentVariableIsSet("ASTRO_OBSERVATION_MAP_ZONE")) {
    reachability(Reachability::Online);
    root->setProperty("autoSelectVisible", false);
    const auto utc = controller.instant()["utcSeconds"];
    controller.chooseMapLocation(48.8566, 2.3522);
    ASTRO_TEST_CHECK(controller.saveSite("Unconfirmed map test"));
    const auto savedIndex = controller.sites().size() - 1;
    controller.resolveMapTimeZone();
    QElapsedTimer timeout;
    timeout.start();
    while (controller.mapTimeZoneBusy() && timeout.elapsed() < 20000) {
      QCoreApplication::processEvents();
      QThread::msleep(10);
    }
    ASTRO_TEST_CHECK(!controller.mapTimeZoneBusy());
    ASTRO_TEST_CHECK(controller.settings()["zone"] == "Europe/Paris");
    ASTRO_TEST_CHECK(controller.mapTimeZoneStatus().isEmpty());
    ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == utc);
    ASTRO_TEST_CHECK(controller.sites()[savedIndex].toMap()["zone"] ==
                     "Europe/Paris");
    ASTRO_TEST_CHECK(
        !controller.sites()[savedIndex].toMap()["mapZoneUnresolved"].toBool());
    // A later explicit site choice cancels application of an old map lookup.
    controller.chooseMapLocation(37.7378, -113.6975);
    controller.resolveMapTimeZone();
    auto explicitSite = controller.settings();
    explicitSite["zone"] = "America/Denver";
    explicitSite["mapZoneUnresolved"] = false;
    ASTRO_TEST_CHECK(controller.changeSite(explicitSite));
    QThread::msleep(650);
    QCoreApplication::processEvents();
    ASTRO_TEST_CHECK(!controller.mapTimeZoneBusy());
    ASTRO_TEST_CHECK(controller.settings()["zone"] == "America/Denver");
    ASTRO_TEST_CHECK(controller.instant()["utcSeconds"] == utc);
  }

  // Exercise legacy migration and preset deletion in a separate settings file.
  const auto applicationName = app.applicationName();
  app.setApplicationName("ObservationPlacesMigration");
  QSettings().setValue("observation/favorites", QVariantList{QVariantMap{
      {"name", "Ancien jardin"}, {"latitude", 45.0}, {"longitude", 3.0},
      {"elevation", 100.0}, {"zone", "Europe/Paris"}}});
  {
    ObservationController places;
    ASTRO_TEST_CHECK(places.sites().size() == 11);
    ASTRO_TEST_CHECK(places.sites().last().toMap()["favorite"].toBool());
    places.toggleFavoriteSite(10);
    places.toggleFavoriteSite(0);
    places.removeSite(1);
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.sites().size() == 10);
    ASTRO_TEST_CHECK(reopened.sites().first().toMap()["favorite"].toBool());
    ASTRO_TEST_CHECK(!reopened.sites().last().toMap()["favorite"].toBool());
    for (const auto &value : reopened.sites())
      ASTRO_TEST_CHECK(value.toMap()["presetId"] != "lyon");
    reopened.removeSite(9);
    for (int i = 8; i >= 0; --i)
      reopened.removeSite(i);
    ObservationController empty;
    ASTRO_TEST_CHECK(empty.sites().isEmpty());
    ASTRO_TEST_CHECK(empty.error().isEmpty());
    ASTRO_TEST_CHECK(empty.saveSite("Nouveau jardin"));
    ASTRO_TEST_CHECK(empty.sites().size() == 1);
    const auto selected = empty.settings();
    const auto instant = empty.instant()["utcSeconds"];
    ASTRO_TEST_CHECK(empty.hasHiddenPresetSites());
    empty.restorePresetSites();
    ASTRO_TEST_CHECK(empty.sites().size() == 11);
    for (const auto &entry : empty.sites())
      ASTRO_TEST_CHECK(!entry.toMap()["label"].toString().isEmpty());
    ASTRO_TEST_CHECK(!empty.hasHiddenPresetSites());
    ASTRO_TEST_CHECK(empty.sites().first().toMap()["name"] == "Nouveau jardin");
    ASTRO_TEST_CHECK(empty.settings() == selected);
    ASTRO_TEST_CHECK(empty.instant()["utcSeconds"] == instant);
    empty.restorePresetSites();
    ObservationController restored;
    ASTRO_TEST_CHECK(restored.sites().size() == 11);
    ASTRO_TEST_CHECK(!restored.hasHiddenPresetSites());
    empty.removeSite(-1);
    empty.toggleFavoriteSite(20);
    ASTRO_TEST_CHECK(empty.sites().size() == 11);
    // A malformed stored coordinate must not silently become zero. Invalid
    // legacy website data is ignored without discarding a valid saved site.
    auto valid = empty.sites().first().toMap();
    auto corrupt = valid;
    corrupt["latitude"] = "broken latitude";
    auto missing = valid;
    missing.remove("longitude");
    valid["website"] = "file:///tmp/untrusted";
    QSettings().setValue("observation/savedSites", QVariantList{corrupt, missing, valid});
    ObservationController checked;
    ASTRO_TEST_CHECK(checked.sites().size() == 11);
    ASTRO_TEST_CHECK(checked.sites().last().toMap()["name"] == valid["name"]);
    ASTRO_TEST_CHECK(checked.sites().last().toMap()["website"].toString().isEmpty());
  }
  // Invalid persisted coordinates must not produce a plausible zero-degree
  // target or overwrite the original settings. A catalogue choice recovers.
  app.setApplicationName("ObservationTargetRecovery");
  const QVariantMap recoverySettings{
      {"latitude", 48.8566}, {"longitude", 2.3522}, {"elevation", 0},
      {"zone", "Europe/Paris"}, {"targetName", "Saved target"},
      {"ra", 42.125}, {"dec", -12.25}};
  for (const auto &id : {QString("manual"), QString("manual:test"),
                         QString("cds:test")}) {
    for (const auto &coordinate : {QString("ra"), QString("dec")}) {
      for (const auto &invalid : {QVariant(), QVariant("broken coordinate"),
                                  QVariant(400.0)}) {
        auto stored = recoverySettings;
        stored["target"] = id;
        if (invalid.isValid())
          stored[coordinate] = invalid;
        else
          stored.remove(coordinate);
        QSettings().setValue("observation/settings", stored);
        ObservationController reopened;
        ASTRO_TEST_CHECK(!reopened.error().isEmpty());
        ASTRO_TEST_CHECK(reopened.samples().isEmpty());
        ASTRO_TEST_CHECK(reopened.result().isEmpty());
        const auto placesBeforeRecovery = reopened.sites();
        ASTRO_TEST_CHECK(!reopened.saveSite("Place before recovery"));
        ASTRO_TEST_CHECK(reopened.sites() == placesBeforeRecovery);
        reopened.toggleFavoriteTarget();
        ASTRO_TEST_CHECK(!reopened.error().isEmpty());
        ASTRO_TEST_CHECK(reopened.favoriteTargets().isEmpty());
        ASTRO_TEST_CHECK(reopened.setReferenceZone("site"));
        ASTRO_TEST_CHECK(QSettings().value("observation/settings").toMap() == stored);
        ASTRO_TEST_CHECK(!reopened.selectTarget(id));
        ASTRO_TEST_CHECK(reopened.selectTarget("NGC1976"));
        ASTRO_TEST_CHECK(reopened.error().isEmpty());
        ASTRO_TEST_CHECK(!reopened.samples().isEmpty());
      }
    }
    // Zero is a valid coordinate when explicitly saved, and exact coordinates
    // survive a restart without rounding.
    for (const auto &coordinates : {QVariantList{0.0, 0.0},
                                    QVariantList{42.125, -12.25}}) {
      auto stored = recoverySettings;
      stored["target"] = id;
      stored["ra"] = coordinates[0];
      stored["dec"] = coordinates[1];
      QSettings().setValue("observation/settings", stored);
      ObservationController reopened;
      ASTRO_TEST_CHECK(reopened.error().isEmpty());
      ASTRO_TEST_CHECK(reopened.settings()["target"] == id);
      ASTRO_TEST_CHECK(reopened.settings()["ra"] == coordinates[0]);
      ASTRO_TEST_CHECK(reopened.settings()["dec"] == coordinates[1]);
    }
  }
  // Unknown catalogue IDs require an explicit choice, even if saved numeric
  // coordinates look valid. No timer, favourite action or failed choice may
  // substitute a different object or overwrite the original settings.
  for (const auto &id : {QString("removed-catalogue-object"), QString()}) {
    auto stored = recoverySettings;
    stored["target"] = id;
    QSettings().setValue("observation/settings", stored);
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.targetUnavailable());
    ASTRO_TEST_CHECK(reopened.error().contains("introuvable"));
    ASTRO_TEST_CHECK(reopened.result().isEmpty());
    ASTRO_TEST_CHECK(!reopened.showNow());
    reopened.toggleFavoriteTarget();
    ASTRO_TEST_CHECK(reopened.targetUnavailable());
    ASTRO_TEST_CHECK(reopened.error().contains("introuvable"));
    ASTRO_TEST_CHECK(!reopened.selectTarget("also-unknown"));
    ASTRO_TEST_CHECK(QSettings().value("observation/settings").toMap() == stored);
    ASTRO_TEST_CHECK(reopened.selectTarget("NGC1976"));
    ASTRO_TEST_CHECK(!reopened.targetUnavailable());
    ASTRO_TEST_CHECK(reopened.error().isEmpty());
    ObservationController recovered;
    ASTRO_TEST_CHECK(!recovered.targetUnavailable());
    ASTRO_TEST_CHECK(recovered.settings()["target"] == "NGC1976");
    QSettings().setValue("observation/settings", stored);
    ObservationController manualRecovery;
    auto coordinates = manualRecovery.settings();
    coordinates["manual"] = true;
    coordinates["ra"] = "invalid";
    ASTRO_TEST_CHECK(!manualRecovery.calculate(coordinates));
    ASTRO_TEST_CHECK(manualRecovery.targetUnavailable());
    ASTRO_TEST_CHECK(QSettings().value("observation/settings").toMap() == stored);
    coordinates["ra"] = 42.125;
    ASTRO_TEST_CHECK(manualRecovery.calculate(coordinates));
    ASTRO_TEST_CHECK(!manualRecovery.targetUnavailable());
    ASTRO_TEST_CHECK(manualRecovery.settings()["target"] == "manual");
  }
  // A missing object plus an invalid site must not trap recovery: choosing a
  // known target leaves the independent site error available for correction.
  auto invalidSite = recoverySettings;
  invalidSite["target"] = "removed-catalogue-object";
  invalidSite["latitude"] = "broken latitude";
  QSettings().setValue("observation/settings", invalidSite);
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(!reopened.selectTarget("NGC1976"));
    ASTRO_TEST_CHECK(!reopened.targetUnavailable());
    ASTRO_TEST_CHECK(!reopened.error().isEmpty());
    ASTRO_TEST_CHECK(reopened.settings()["target"] == "NGC1976");
    ASTRO_TEST_CHECK(reopened.settings()["latitude"] == "broken latitude");
    auto corrected = reopened.settings();
    corrected["latitude"] = 48.8566;
    ASTRO_TEST_CHECK(reopened.changeSite(corrected));
    ASTRO_TEST_CHECK(reopened.error().isEmpty());
  }
  app.setApplicationName(applicationName);
  return 0;
}
