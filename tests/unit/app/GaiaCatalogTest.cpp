#include "GaiaCatalog.hpp"
#include "Check.hpp"
#include "DeepSkySettings.hpp"
#include "WcsGeometry.hpp"
#include <QFile>
#include <QString>
#include <cmath>
using namespace astro::app;
int main() {
  QByteArray csv = "source_id,ra,dec,pmra,pmdec,ref_epoch,phot_g_mean_mag,teff_"
                   "gspphot,has_xp_sampled\n";
  for (int n = 0; n < 10; ++n)
    csv += QByteArray::number(3017364063330718080ULL + n) +
           ",359.999,60,1000,0,2016,12,5778,true\n";
  auto stars = parseGaiaCatalog(csv, 2026);
  ASTRO_TEST_CHECK(stars.size() == 10 &&
                   stars.front().id == "3017364063330718080");
  ASTRO_TEST_CHECK(stars.front().ra < .01 &&
                   std::abs(std::remainder(stars.front().ra - 359.999, 360.) -
                            10000. / 3600000 / .5) < 1e-6);
  auto restored = readCatalogValues(catalogValues(stars));
  ASTRO_TEST_CHECK(restored.size() == stars.size() &&
                   restored.front().id == stars.front().id &&
                   restored.front().ra == stars.front().ra);
  bool rejected = false;
  try {
    parseGaiaCatalog("ra,dec\n1,2", 2026);
  } catch (const std::exception &) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  rejected = false;
  auto bad = csv;
  bad.replace("359.999", "-1");
  try {
    parseGaiaCatalog(bad, 2026);
  } catch (const std::exception &) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  QFile fixture(QStringLiteral(
      ASTRO_SOURCE_DIR
      "/tests/unit/fixtures/gaia-dr3-xp-3017364063330718080.xml"));
  ASTRO_TEST_CHECK(fixture.open(QIODevice::ReadOnly));
  auto bytes = fixture.readAll();
  auto spectrum = parseGaiaSpectrum(bytes, "3017364063330718080");
  ASTRO_TEST_CHECK(spectrum.size() == 343 && spectrum.front()[0] == 336 &&
                   spectrum.back()[0] == 1020);
  ASTRO_TEST_CHECK(spectrum[100][1] > 0);
  rejected = false;
  try {
    parseGaiaSpectrum(bytes, "3017364063330718081");
  } catch (const std::exception &) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  rejected = false;
  auto altered = bytes;
  altered.replace("unit=\"nm\"", "unit=\"Angstrom\"");
  try {
    parseGaiaSpectrum(altered, "3017364063330718080");
  } catch (const std::exception &) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  auto response = astro::processing::standardResponse();
  auto expectedColors = astro::processing::referenceColors(
      spectrum, response, astro::processing::solarSpectrum());
  for (double value : expectedColors)
    ASTRO_TEST_CHECK(std::isfinite(value) && value > 0);
  QFile archive(QStringLiteral(ASTRO_SOURCE_DIR
                               "/tests/unit/fixtures/gaia-dr3-xp-batch.zip"));
  ASTRO_TEST_CHECK(archive.open(QIODevice::ReadOnly));
  auto zip = archive.readAll();
  QStringList ids{"3017364063330718080", "3017360936587007616",
                  "3017187866581304832"};
  auto batch = parseGaiaSpectraArchive(zip, ids);
  ASTRO_TEST_CHECK(batch.size() == 3 && batch[ids[0]] == spectrum);
  rejected = false;
  auto corrupt = zip;
  corrupt[100] = char(corrupt[100] ^ 0x12);
  try {
    parseGaiaSpectraArchive(corrupt, ids);
  } catch (const std::exception &) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  rejected = false;
  auto badOffset = zip;
  const int central = badOffset.indexOf(QByteArray::fromHex("504b0102"));
  ASTRO_TEST_CHECK(central >= 0);
  badOffset.replace(central + 42, 4, QByteArray::fromHex("ffffff7f"));
  try {
    parseGaiaSpectraArchive(badOffset, ids);
  } catch (const std::exception &) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  rejected = false;
  try {
    parseGaiaSpectraArchive(zip, QStringList{ids[0]});
  } catch (const std::exception &) {
    rejected = true;
  }
  ASTRO_TEST_CHECK(rejected);
  astro::processing::FloatImage image{2, 1, {-.2f, .3f, 2.1f, 1, 2, 3}};
  auto key = linearImageKey(image);
  ASTRO_TEST_CHECK(key.size() == 64);
  image.rgb[0] = -.21f;
  ASTRO_TEST_CHECK(linearImageKey(image) != key);
  astro::recording::CelestialWcs w{{83.8, -5.3}, {51, 41}, {-.001, 0, 0, .001}};
  auto settings = defaultDeepSkySettings();
  settings["rotation"] = 1;
  settings["mirrorHorizontal"] = true;
  auto transformed = finishWcs(w, 100, 80, settings);
  auto expected = w.sky(12.3, 41.7), actual = transformed.sky(41.7, 12.3);
  ASTRO_TEST_CHECK(expected && actual &&
                   std::abs((*expected)[0] - (*actual)[0]) < 1e-12 &&
                   std::abs((*expected)[1] - (*actual)[1]) < 1e-12);
  settings = defaultDeepSkySettings();
  auto geometry = settings["geometry"].toMap();
  geometry["crop"] = true;
  geometry["x"] = 10;
  geometry["y"] = 20;
  geometry["width"] = 70;
  geometry["height"] = 50;
  geometry["scale"] = .51;
  settings["geometry"] = geometry;
  transformed = finishWcs(w, 100, 80, settings);
  expected = w.sky(40, 45);
  actual = transformed.sky((40 - 10 + .5) * (36. / 70) - .5,
                           (45 - 20 + .5) * (26. / 50) - .5);
  ASTRO_TEST_CHECK(expected && actual &&
                   std::abs((*expected)[0] - (*actual)[0]) < 1e-12 &&
                   std::abs((*expected)[1] - (*actual)[1]) < 1e-12);
}
