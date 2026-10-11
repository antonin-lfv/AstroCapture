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
  ObservationController controller;
  ASTRO_TEST_CHECK(controller.catalogCount() > 13000);
  ASTRO_TEST_CHECK(controller.error().isEmpty());
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
  auto old = controller.result();
  opts["latitude"] = 91;
  ASTRO_TEST_CHECK(!controller.calculate(opts));
  ASTRO_TEST_CHECK(controller.result() == old);
  opts["latitude"] = 48.8566;
  ASTRO_TEST_CHECK(controller.calculate(opts));
  const auto utc = controller.instant()["utcSeconds"].toLongLong();
  controller.chooseMapLocation(-33, 151);
  ASTRO_TEST_CHECK(controller.settings()["zone"] == "UTC");
  ASTRO_TEST_CHECK(controller.instant()["utcSeconds"].toLongLong() == utc);
  ASTRO_TEST_CHECK(controller.saveSite("Mon jardin"));
  {
    ObservationController reopened;
    ASTRO_TEST_CHECK(reopened.sites().last().toMap()["name"] == "Mon jardin");
  }
  controller.removeSite(controller.sites().size() - 1);
  opts = controller.sites()[0].toMap();
  opts["date"] = "2026-12-12";
  opts["minimum"] = 30;
  opts["sunLimit"] = -18;
  ASTRO_TEST_CHECK(controller.calculate(opts));
  ASTRO_TEST_CHECK(controller.selectTarget("NGC1976"));
  controller.search("");
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
    ASTRO_TEST_CHECK(controller.selectTarget(
        controller.searchResults().first().toMap()["id"].toString()));
  }

  return 0;
}
