#include "BlindSolver.hpp"
#include "AstrometrySettings.hpp"
#include "Check.hpp"
#include "DeepSkyController.hpp"
#include "DeepSkySettings.hpp"
#include <QDebug>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
using namespace astro;
void wait(app::DeepSkyController &c) {
  QElapsedTimer timer;
  timer.start();
  while (c.busy() && timer.elapsed() < 200000) {
    QCoreApplication::processEvents();
    QThread::msleep(5);
  }
  ASTRO_TEST_CHECK(!c.busy());
}
int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  QTemporaryDir dir;
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
  QCoreApplication::setOrganizationName("AstroTest");
  QCoreApplication::setApplicationName("BlindSolver");
  bool refused = false;
  try {
    app::setBlindIndexDirectory(dir.filePath("missing"));
  } catch (const std::exception &) {
    refused = true;
  }
  ASTRO_TEST_CHECK(refused);
  app::setBlindIndexDirectory(dir.path());
  ASTRO_TEST_CHECK(!app::blindSolverConfiguration()["ready"].toBool());
  processing::FloatImage tiny{4, 4, std::vector<float>(48, .01f)};
  std::stop_source stop;
  stop.request_stop();
  refused = false;
  try {
    app::solveBlindField(tiny, {}, stop.get_token());
  } catch (const std::exception &) {
    refused = true;
  }
  ASTRO_TEST_CHECK(refused);
  if (qEnvironmentVariableIsEmpty("ASTRO_BLIND_REAL_IMAGE"))
    return 0;
  app::setBlindIndexDirectory(qEnvironmentVariable("ASTRO_BLIND_INDEXES"));
  ASTRO_TEST_CHECK(app::blindSolverConfiguration()["ready"].toBool());
  app::DeepSkyController c;
  c.addFiles(
      {QUrl::fromLocalFile(qEnvironmentVariable("ASTRO_BLIND_REAL_IMAGE"))});
  wait(c);
  c.integrate(true);
  wait(c);
  ASTRO_TEST_CHECK(c.hasResult());
  const auto original = c.sessionSnapshot().images["linear"];
  // Deliberately wrong hints: blind mode must ignore both these and FITS WCS.
  c.solveAstrometry({{"blind", true},
                     {"ra", 210},
                     {"dec", 50},
                     {"scale", .1},
                     {"sipOrder", 3},
                     {"magnitude", 16}});
  wait(c);
  qInfo() << c.status() << c.astrometry()["matches"] << c.astrometry()["rms"];
  ASTRO_TEST_CHECK(c.astrometry()["verified"].toBool() &&
                   c.astrometry()["matches"].toInt() >= 30 &&
                   c.astrometry()["sipOrder"].toInt() == 3);
  ASTRO_TEST_CHECK(std::abs(c.astrometry()["ra"].toDouble() - 83.82) < .02 &&
                   std::abs(c.astrometry()["dec"].toDouble() + 5.39) < .02);
  ASTRO_TEST_CHECK(c.sessionSnapshot().images["linear"] == original &&
                   app::DeepSkyController::validSession(c.sessionSnapshot()));
  const auto path = dir.filePath("solved.fits");
  c.exportImage(QUrl::fromLocalFile(path), true);
  wait(c);
  const auto read = recording::readAstroImage(path.toStdString(), 1);
  ASTRO_TEST_CHECK(read.samples == original->rgb && read.wcs &&
                   read.wcs->sipOrder == 3);
  c.solveAstrometry({{"blind", true}, {"sipOrder", 8}, {"magnitude", 16}});
  wait(c);
  ASTRO_TEST_CHECK(c.astrometry()["verified"].toBool() &&
                   c.sessionSnapshot().images["linear"] == original);
}
