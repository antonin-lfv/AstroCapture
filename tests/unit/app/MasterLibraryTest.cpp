#include "MasterLibrary.hpp"
#include "Check.hpp"
#include "DeepSkyController.hpp"
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <astro/processing/DeepSky.hpp>
#include <cmath>
using namespace astro;
void wait(app::DeepSkyController &c) {
  QElapsedTimer t;
  t.start();
  while (c.busy() && t.elapsed() < 20000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  ASTRO_TEST_CHECK(!c.busy());
}
int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  QTemporaryDir d;
  QCoreApplication::setOrganizationName("AstroTest");
  QCoreApplication::setApplicationName("MasterLibrary");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, d.path());
  QSettings().setValue("calibration/libraryDirectory", d.filePath("masters"));
  recording::AstroImage image;
  image.width = image.height = 64;
  image.bits = 32;
  image.samples.resize(4096);
  image.camera = "Test sensor";
  image.binX = image.binY = 1;
  image.hasGain = image.hasOffset = image.hasTemperature = true;
  image.gain = 100;
  image.offset = 10;
  image.temperature = -10;
  image.exposure = 30;
  image.filter = "L";
  auto write = [&](QString name, float base, float slope = 0) {
    auto a = image;
    for (int y = 0; y < 64; ++y)
      for (int x = 0; x < 64; ++x)
        a.samples[y * 64 + x] = base + slope * x / 63;
    const auto p = d.filePath(name + ".fits");
    recording::writeAstroFits(p.toStdString(), a);
    return QUrl::fromLocalFile(p);
  };
  auto clean = image;
  clean.samples.assign(4096, .1);
  auto cosmic = clean;
  cosmic.samples[50] = 100;
  const auto robust = processing::masterFrame(
      3, [&](int n) { return n == 2 ? cosmic : clean; }, false, {}, true);
  ASTRO_TEST_CHECK(std::abs(robust.samples[50] - .1) < 1e-7 &&
                   robust.masterRejected == 1);
  const auto light = write("light", .6, .2), dark1 = write("dark1", .04),
             dark2 = write("dark2", .06);
  app::DeepSkyController c;
  c.addFiles({light}, "Light");
  wait(c);
  c.addFiles({dark1, dark2}, "Dark");
  wait(c);
  c.createCalibrationMaster("Dark", "30s −10C");
  wait(c);
  ASTRO_TEST_CHECK(c.calibrationMasters().size() == 1);
  auto dark = c.calibrationMasters().front().toMap();
  ASTRO_TEST_CHECK(dark["masterCount"] == 2 && dark["compatibility"] == 0);
  auto raw = recording::readAstroImage(dark["path"].toString().toStdString());
  ASTRO_TEST_CHECK(raw.masterRole == "Dark" && raw.masterCount == 2 &&
                   raw.camera == image.camera && raw.binX == 1 &&
                   raw.hasOffset && std::abs(raw.samples[0] - .05) < 1e-7);
  auto m = dark;
  auto rows = c.frames();
  auto r = rows.front().toMap();
  r["gain"] = 200;
  rows[0] = r;
  ASTRO_TEST_CHECK(app::masterCompatibility(m, rows)["compatibility"] == 2);
  r.remove("gain");
  rows[0] = r;
  ASTRO_TEST_CHECK(app::masterCompatibility(m, rows)["compatibility"] == 1);
  r["gain"] = 100;
  r["temperature"] = -13;
  rows[0] = r;
  ASTRO_TEST_CHECK(app::masterCompatibility(m, rows)["compatibility"] == 2);
  c.useCalibrationMaster(dark["id"].toString());
  wait(c);
  ASTRO_TEST_CHECK(c.frames().back().toMap()["selected"].toBool() &&
                   !c.frames()[1].toMap()["selected"].toBool());
  // Bias-correct flats once; applying a saved normalized flat must not subtract
  // again.
  image.exposure = .01;
  const auto bias = write("bias", .02);
  c.addFiles({bias}, "Bias");
  wait(c);
  image.exposure = 2;
  const auto flat1 = write("flat1", .52, .4), flat2 = write("flat2", .62, .48);
  c.addFiles({flat1, flat2}, "Flat");
  wait(c);
  c.createCalibrationMaster("Flat", "L corrected");
  wait(c);
  ASTRO_TEST_CHECK(c.calibrationMasters().size() == 2);
  auto flat = c.calibrationMasters().back().toMap();
  ASTRO_TEST_CHECK(flat["masterFlat"].toBool() && flat["compatibility"] == 0);
  c.useCalibrationMaster(flat["id"].toString());
  wait(c);
  c.integrate(false, 3, false, false);
  wait(c);
  ASTRO_TEST_CHECK(c.hasResult());
  const auto result = c.sessionSnapshot().images["linear"];
  const auto normalized =
      recording::readAstroImage(flat["path"].toString().toStdString());
  for (int x = 0; x < 64; ++x) {
    const auto expected = (.6f + .2f * x / 63 - .05f) / normalized.samples[x];
    ASTRO_TEST_CHECK(std::abs(result->rgb[x * 3] - expected) < 2e-6);
  }
  ASTRO_TEST_CHECK(app::DeepSkyController::validSession(c.sessionSnapshot()));
  std::stop_source stopped;
  stopped.request_stop();
  bool refused = false;
  try {
    app::storeCalibrationMaster(raw, "cancelled", stopped.get_token());
  } catch (const std::exception &) {
    refused = true;
  }
  ASTRO_TEST_CHECK(refused && app::calibrationMasterInventory().size() == 2);
  // A missing header can only be overridden after an explicit review; a known
  // mismatch cannot.
  image.exposure = 30;
  image.hasGain = false;
  const auto unknown = write("unknown-light", .5, .1);
  app::DeepSkyController other;
  other.addFiles({unknown}, "Light");
  wait(other);
  other.useCalibrationMaster(dark["id"].toString(), false);
  wait(other);
  ASTRO_TEST_CHECK(other.frames().size() == 1);
  other.useCalibrationMaster(dark["id"].toString(), true);
  wait(other);
  ASTRO_TEST_CHECK(other.frames().size() == 2);
  QFile altered(flat["path"].toString());
  ASTRO_TEST_CHECK(altered.open(QIODevice::Append));
  altered.write("tamper");
  altered.close();
  c.integrate(false);
  wait(c);
  ASTRO_TEST_CHECK(c.sessionSnapshot().images["linear"] == result);
  app::removeCalibrationMaster(dark["id"].toString());
  ASTRO_TEST_CHECK(app::calibrationMasterInventory().size() == 1 &&
                   QFile::exists(dark["path"].toString()));
}
