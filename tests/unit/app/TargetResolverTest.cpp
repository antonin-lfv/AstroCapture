#include "TargetResolver.hpp"
#include "Check.hpp"
#include <QFile>
#include <cmath>
#include <limits>
using namespace astro::app;
int main() {
  QFile fixture(QStringLiteral(ASTRO_SOURCE_DIR
                               "/tests/unit/fixtures/cds-sesame-m42.xml"));
  ASTRO_TEST_CHECK(fixture.open(QIODevice::ReadOnly));
  const auto bytes = fixture.readAll();
  const auto target = parseTargetPosition(bytes, "M42");
  ASTRO_TEST_CHECK(target["name"].toString() == "M 42" &&
                   std::abs(target["ra"].toDouble() - 83.8201) < 1e-10 &&
                   std::abs(target["dec"].toDouble() + 5.3876) < 1e-10);
  ASTRO_TEST_CHECK(target["source"].toString() == "SIMBAD · CDS" &&
                   target["resolver"].toString().contains("Simbad"));
  const auto url = targetResolverUrl(" NAME + M42 & M31 #é ");
  ASTRO_TEST_CHECK(url.scheme() == "https" && url.fragment().isEmpty() &&
                   url.query(QUrl::FullyDecoded) == "NAME + M42 & M31 #é");
  ASTRO_TEST_CHECK(url.toEncoded().contains("%26") &&
                   url.toEncoded().contains("%23") &&
                   url.toEncoded().contains("%2B"));
  auto refused = [](const QByteArray &xml, const QString &name = "M42") {
    bool rejected = false;
    try {
      parseTargetPosition(xml, name);
    } catch (const std::exception &) {
      rejected = true;
    }
    ASTRO_TEST_CHECK(rejected);
  };
  refused(bytes, "M31");
  refused("<Sesame><Target><name>M42</name></Target></Sesame>");
  auto invalid = bytes;
  invalid.replace("83.8201", "nan");
  refused(invalid);
  invalid = bytes;
  invalid.replace("83.8201", "360");
  refused(invalid);
  invalid = bytes;
  invalid.replace("-5.3876", "-91");
  refused(invalid);
  invalid = bytes;
  invalid.replace("<jradeg>83.8201</jradeg>",
                  "<jradeg>83.8201</jradeg><jradeg>1</jradeg>");
  refused(invalid);
  refused("<!DOCTYPE Sesame [<!ENTITY test "
          "'M42'>]><Sesame><Target><name>&test;</name></Target></Sesame>");
  refused("<HTML>error</HTML>");
  refused(bytes.left(bytes.size() / 2));
  refused(QByteArray(65537, 'x'));
  const QByteArray fallback =
      "<Sesame><Target><name>M42</name><Resolver "
      "name='Simbad'><INFO>Unavailable</INFO></Resolver><Resolver "
      "name='NED'><jradeg>83.82</jradeg><jdedeg>-5.39</jdedeg></Resolver></"
      "Target></Sesame>";
  ASTRO_TEST_CHECK(parseTargetPosition(fallback, "M42")["source"] == "NED");
  auto ambiguous = fallback;
  ambiguous.replace("<INFO>Unavailable</INFO>",
                    "<jradeg>80</jradeg><jdedeg>-5</jdedeg>");
  refused(ambiguous);
  auto crossResolver = fallback;
  crossResolver.replace("<INFO>Unavailable</INFO>", "<jradeg>80</jradeg>");
  refused(crossResolver);
  ASTRO_TEST_CHECK(rightAscensionText(0) == "00:00:00.00");
  ASTRO_TEST_CHECK(rightAscensionText(359.999999999) == "00:00:00.00");
  ASTRO_TEST_CHECK(rightAscensionText(83.8201) == "05:35:16.82");
  ASTRO_TEST_CHECK(declinationText(-5.3876) == "-05:23:15.4");
  ASTRO_TEST_CHECK(declinationText(-0.) == "-00:00:00.0");
  ASTRO_TEST_CHECK(declinationText(90) == "+90:00:00.0");
  ASTRO_TEST_CHECK(
      rightAscensionText(std::numeric_limits<double>::quiet_NaN()).isEmpty());
  ASTRO_TEST_CHECK(declinationText(91).isEmpty());
}
