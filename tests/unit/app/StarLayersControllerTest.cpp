#include "Check.hpp"
#include "DeepSkyController.hpp"
#include "DeepSkyEdits.hpp"
#include "DeepSkySettings.hpp"
#include "ImageDocument.hpp"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>
#include <astro/recording/AstroImage.hpp>
#include <cmath>
using namespace astro;
void wait(app::DeepSkyController &c) {
  QElapsedTimer t;
  t.start();
  while (c.busy() && t.elapsed() < 30000) {
    QCoreApplication::processEvents();
    QThread::msleep(1);
  }
  ASTRO_TEST_CHECK(!c.busy());
}
int main(int argc, char **argv) {
  QGuiApplication application(argc, argv);
  QTemporaryDir dir;
  QCoreApplication::setOrganizationName("AstroTest");
  QCoreApplication::setApplicationName("StarLayers");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, dir.path());
  recording::AstroImage a;
  a.width = a.height = 256;
  a.channels = 3;
  a.bits = 32;
  a.samples.resize(256 * 256 * 3);
  for (int y = 0; y < 256; ++y)
    for (int x = 0; x < 256; ++x)
      for (int k = 0; k < 3; ++k) {
        double v =
            .02 +
            .1 * std::exp(-(std::pow(x - 130., 2) + std::pow(y - 160., 2)) /
                          1200);
        for (const auto &s : std::vector<std::array<int, 2>>{
                 {40, 40}, {100, 40}, {210, 60}, {50, 130}, {200, 210}})
          v += (k == 0   ? .4
                : k == 1 ? .3
                         : .2) *
               std::exp(-4 * std::log(2.) *
                        (std::pow(x - s[0], 2) + std::pow(y - s[1], 2)) / 9);
        a.samples[(y * 256 + x) * 3 + k] = v;
      }
  a.wcs =
      recording::CelestialWcs{{83.8, -5.4}, {128, 128}, {.0003, 0, 0, .0003}};
  a.wcs->sipOrder = 3;
  a.wcs->sipA[12] = 1e-6;
  const auto input = dir.filePath("source.fits");
  recording::writeAstroFits(input.toStdString(), a);
  app::DeepSkyController c;
  c.setProjectCacheDirectory(dir.filePath("cache"));
  c.addFiles({QUrl::fromLocalFile(input)});
  wait(c);
  c.integrate(true);
  wait(c);
  const auto original = c.sessionSnapshot().images["linear"];
  auto s = app::defaultDeepSkySettings();
  s["black"] = .01;
  s["white"] = .8;
  s["midtones"] = .12;
  c.apply(s);
  wait(c);
  const auto normal =
      c.processingDocument().value<app::ImageDocument>().pixels->rgb;
  s["starSeparation"] = true;
  c.apply(s);
  wait(c);
  ASTRO_TEST_CHECK(
      c.hasStarLayers() && c.advancedReport()["starsAccepted"].toInt() >= 4 &&
      c.processingDocument().value<app::ImageDocument>().pixels->rgb == normal);
  app::DeepSkyEdits edits;
  ASTRO_TEST_CHECK(edits.resetSession(s));
  auto reduced = s;
  reduced["starsGain"] = .4;
  ASTRO_TEST_CHECK(edits.applyValues(reduced));
  c.apply(edits.values());
  wait(c);
  const auto reducedImage = c.sessionSnapshot().images["finished"];
  for (int layer : {1, 2}) {
    c.setStarLayerView(layer);
    const auto p =
        c.sessionSnapshot().images[layer == 1 ? "stars" : "starBackground"];
    const auto output = dir.filePath(QString("layer%1.fits").arg(layer));
    c.exportStarLayer(QUrl::fromLocalFile(output), layer);
    wait(c);
    auto written = recording::readAstroImage(output.toStdString());
    ASTRO_TEST_CHECK(written.samples == p->rgb && written.wcs &&
                     written.wcs->sipOrder == 3);
    ASTRO_TEST_CHECK(
        c.processingDocument().value<app::ImageDocument>().pixels ==
        reducedImage);
  }
  auto snapshot = c.sessionSnapshot();
  ASTRO_TEST_CHECK(app::DeepSkyController::validSession(snapshot));
  for (std::size_t i = 0; i < reducedImage->rgb.size(); ++i)
    ASTRO_TEST_CHECK(std::abs(snapshot.images["stars"]->rgb[i] +
                              snapshot.images["starBackground"]->rgb[i] -
                              reducedImage->rgb[i]) < 1e-7);
  c.inspect(0);
  wait(c);
  ASTRO_TEST_CHECK(c.hasStarLayers());
  c.restoreResult();
  c.setStarLayerView(2);
  const auto project = QUrl::fromLocalFile(dir.filePath("layers.astroproject"));
  c.savePortableProject(project);
  wait(c);
  app::DeepSkyController reopened;
  reopened.setProjectCacheDirectory(dir.filePath("reopened"));
  reopened.loadProject(project);
  wait(reopened);
  ASTRO_TEST_CHECK(reopened.hasStarLayers() && reopened.starLayerView() == 2 &&
                   reopened.sessionSnapshot().images["stars"]->rgb ==
                       snapshot.images["stars"]->rgb);
  edits.undo();
  c.apply(edits.values());
  wait(c);
  ASTRO_TEST_CHECK(
      c.processingDocument().value<app::ImageDocument>().pixels->rgb ==
          normal &&
      c.sessionSnapshot().images["linear"] == original);
  edits.redo();
  c.apply(edits.values());
  wait(c);
  ASTRO_TEST_CHECK(
      c.processingDocument().value<app::ImageDocument>().pixels->rgb ==
      reducedImage->rgb);
  auto invalid = reduced;
  invalid["maskType"] = 2;
  ASTRO_TEST_CHECK(!app::checkedDeepSkySettings(invalid));
  auto old = app::defaultDeepSkySettings();
  for (const auto &key : QStringList{
           "starSeparation", "starPsfProfile", "starFwhm", "starBeta",
           "starLimit", "starFitTolerance", "starSaturationLimit", "starsGain",
           "starsSaturation", "starsMidtones", "starBackgroundGain",
           "starBackgroundSaturation", "starBackgroundDenoise"})
    old.remove(key);
  ASTRO_TEST_CHECK(app::checkedDeepSkySettings(old));
  reduced["rotation"] = 1;
  auto geometry = app::defaultGeometry();
  geometry["crop"] = true;
  geometry["x"] = .1;
  geometry["y"] = .1;
  geometry["width"] = .8;
  geometry["height"] = .8;
  reduced["geometry"] = geometry;
  c.apply(reduced);
  wait(c);
  ASTRO_TEST_CHECK(app::DeepSkyController::validSession(c.sessionSnapshot()) &&
                   c.sessionSnapshot().images["stars"]->width ==
                       c.sessionSnapshot().images["finished"]->width);
}
