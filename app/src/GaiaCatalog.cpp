#include "GaiaCatalog.hpp"
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QXmlStreamReader>
#include <QtEndian>
#include <algorithm>
#include <bit>
#include <cmath>
#include <set>
#include <stdexcept>
#include <zlib.h>
namespace astro::app {
namespace {
double number(QString text, double fallback = NAN) {
  bool ok = false;
  const double v = text.trimmed().toDouble(&ok);
  return ok && std::isfinite(v) ? v : fallback;
}
} // namespace
QByteArray linearImageKey(const processing::FloatImage &image) {
  QCryptographicHash h(QCryptographicHash::Sha256);
  h.addData(QByteArray::number(image.width) + "x" +
            QByteArray::number(image.height) + ":Float32RGB:");
  h.addData(QByteArrayView(reinterpret_cast<const char *>(image.rgb.data()),
                           qsizetype(image.rgb.size() * sizeof(float))));
  return h.result().toHex();
}
std::vector<processing::CatalogStar> parseGaiaCatalog(const QByteArray &bytes,
                                                      double observationEpoch) {
  if (bytes.size() > 8 * 1024 * 1024 || !std::isfinite(observationEpoch) ||
      observationEpoch < 1900 || observationEpoch > 2200)
    throw std::runtime_error("Invalid Gaia catalogue or observation epoch.");
  const auto lines = QString::fromUtf8(bytes).split('\n');
  QStringList columns;
  std::vector<processing::CatalogStar> stars;
  const QRegularExpression idPattern("^[0-9]{1,20}$");
  for (auto line : lines) {
    line = line.trimmed();
    if (line.isEmpty() || line.startsWith('#'))
      continue;
    if (columns.isEmpty()) {
      columns = line.split(',');
      for (auto &c : columns)
        c = c.trimmed().remove('"');
      for (auto c : {"source_id", "ra", "dec", "phot_g_mean_mag"})
        if (!columns.contains(c))
          throw std::runtime_error(
              "Gaia CSV requires source_id,ra,dec,phot_g_mean_mag. Optional: "
              "teff_gspphot,has_xp_sampled,pmra,pmdec,ref_epoch.");
      continue;
    }
    const auto values = line.split(',');
    if (values.size() != columns.size())
      throw std::runtime_error("Invalid Gaia CSV row.");
    auto field = [&](const char *name) {
      const int i = columns.indexOf(name);
      return i < 0 ? QString{} : QString(values[i]).trimmed().remove('"');
    };
    processing::CatalogStar star;
    star.id = field("source_id").toStdString();
    star.ra = number(field("ra"));
    star.dec = number(field("dec"));
    star.magnitude = number(field("phot_g_mean_mag"));
    star.temperature = number(field("teff_gspphot"), 0);
    star.spectrum = field("has_xp_sampled").toLower() == "true" ||
                    field("has_xp_sampled") == "1";
    if (!idPattern.match(QString::fromStdString(star.id)).hasMatch() ||
        !std::isfinite(star.ra) || star.ra < 0 || star.ra >= 360 ||
        !std::isfinite(star.dec) || std::abs(star.dec) > 90 ||
        !std::isfinite(star.magnitude) || std::abs(star.magnitude) > 50 ||
        star.temperature < 0 || star.temperature > 100000)
      throw std::runtime_error(
          "Invalid Gaia star coordinates, identifier or photometry.");
    // Gaia pmra includes cos(dec). Propagate on the sphere, including poles.
    const double pmra = number(field("pmra"), 0),
                 pmdec = number(field("pmdec"), 0),
                 refEpoch = number(field("ref_epoch"), 2016);
    if (std::abs(pmra) > 1e6 || std::abs(pmdec) > 1e6 || refEpoch < 1900 ||
        refEpoch > 2200)
      throw std::runtime_error("Invalid Gaia proper motion.");
    constexpr double rad = 3.141592653589793 / 180.;
    const double a = star.ra * rad, d = star.dec * rad,
                 f = (observationEpoch - refEpoch) * rad / 3600000.;
    const double x = std::cos(d) * std::cos(a) +
                     f * (-pmra * std::sin(a) -
                          pmdec * std::sin(d) * std::cos(a)),
                 y = std::cos(d) * std::sin(a) +
                     f * (pmra * std::cos(a) -
                          pmdec * std::sin(d) * std::sin(a)),
                 z = std::sin(d) + f * pmdec * std::cos(d);
    star.ra = std::fmod(std::atan2(y, x) / rad + 360., 360.);
    star.dec = std::atan2(z, std::hypot(x, y)) / rad;
    stars.push_back(star);
    if (stars.size() > 5000)
      throw std::runtime_error("Gaia catalogues are limited to 5000 stars.");
  }
  if (stars.size() < 8)
    throw std::runtime_error("At least eight catalogue stars are required.");
  std::sort(stars.begin(), stars.end(),
            [](auto &a, auto &b) { return a.magnitude < b.magnitude; });
  std::set<std::string> ids;
  for (const auto &s : stars)
    if (!ids.insert(s.id).second)
      throw std::runtime_error("Duplicate Gaia source identifier.");
  return stars;
}
QVariantList
catalogValues(const std::vector<processing::CatalogStar> &catalog) {
  QVariantList rows;
  for (const auto &s : catalog)
    rows << QVariant::fromValue(QVariantList{QString::fromStdString(s.id), s.ra,
                                             s.dec, s.magnitude, s.temperature,
                                             s.spectrum});
  return rows;
}
std::vector<processing::CatalogStar>
readCatalogValues(const QVariantList &rows) {
  if (rows.size() > 5000)
    throw std::runtime_error("Invalid saved Gaia catalogue.");
  std::vector<processing::CatalogStar> out;
  std::set<QString> ids;
  for (const auto &row : rows) {
    const auto a = row.toList();
    if (a.size() != 6)
      throw std::runtime_error("Invalid saved Gaia catalogue.");
    for (int n = 1; n < 5; ++n) {
      bool ok = false;
      a[n].toDouble(&ok);
      if (!ok)
        throw std::runtime_error("Invalid saved Gaia catalogue.");
    }
    if (a[0].typeId() != QMetaType::QString || a[5].typeId() != QMetaType::Bool)
      throw std::runtime_error("Invalid saved Gaia catalogue.");
    processing::CatalogStar s{a[0].toString().toStdString(),
                              a[1].toDouble(),
                              a[2].toDouble(),
                              a[3].toDouble(),
                              a[4].toDouble(),
                              a[5].toBool()};
    if (!QRegularExpression("^[0-9]{1,20}$")
             .match(a[0].toString())
             .hasMatch() ||
        !std::isfinite(s.ra) || !std::isfinite(s.dec) ||
        !std::isfinite(s.magnitude) || !std::isfinite(s.temperature) ||
        s.ra < 0 || s.ra >= 360 || std::abs(s.dec) > 90 ||
        std::abs(s.magnitude) > 50 || s.temperature < 0 ||
        s.temperature > 100000 || !ids.insert(a[0].toString()).second)
      throw std::runtime_error("Invalid saved Gaia catalogue.");
    out.push_back(s);
  }
  return out;
}
processing::Spectrum parseGaiaSpectrum(const QByteArray &bytes,
                                       const QString &sourceId) {
  QXmlStreamReader xml(bytes);
  QString id, encoded;
  QStringList names, types, units;
  bool binary2 = false;
  while (!xml.atEnd()) {
    xml.readNext();
    if (!xml.isStartElement())
      continue;
    const auto a = xml.attributes();
    if (xml.name() == "PARAM" && a.value("name") == "source_id")
      id = a.value("value").toString();
    if (xml.name() == "FIELD") {
      names << a.value("name").toString();
      types << a.value("datatype").toString();
      units << a.value("unit").toString();
    }
    if (xml.name() == "BINARY2")
      binary2 = true;
    if (xml.name() == "STREAM") {
      if (a.value("encoding") != "base64" || a.hasAttribute("href"))
        throw std::runtime_error("Unsupported Gaia spectrum encoding.");
      encoded = xml.readElementText();
    }
  }
  if (xml.hasError() || id != sourceId || !binary2 ||
      names != QStringList{"wavelength", "flux", "flux_error"} ||
      types != QStringList{"double", "float", "float"} ||
      units.value(0) != "nm" || units.value(1) != "W.m**-2.nm**-1")
    throw std::runtime_error(
        "Gaia XP spectrum identifier, units or schema did not validate.");
  const auto binary = QByteArray::fromBase64(
      encoded.remove(QRegularExpression("\\s+")).toLatin1(),
      QByteArray::AbortOnBase64DecodingErrors);
  if (binary.isEmpty() || binary.size() % 17 || binary.size() / 17 > 10000)
    throw std::runtime_error("Invalid Gaia XP binary spectrum.");
  processing::Spectrum spectrum;
  double previous = 0;
  for (int i = 0; i < binary.size(); i += 17) {
    const auto *p = reinterpret_cast<const uchar *>(binary.constData() + i);
    if (p[0] & 0xe0)
      throw std::runtime_error("Gaia XP contains null spectral samples.");
    const double wavelength =
                     std::bit_cast<double>(qFromBigEndian<quint64>(p + 1)),
                 flux = std::bit_cast<float>(qFromBigEndian<quint32>(p + 9)),
                 error = std::bit_cast<float>(qFromBigEndian<quint32>(p + 13));
    if (!std::isfinite(wavelength) || !std::isfinite(flux) ||
        !std::isfinite(error) || error < 0 || wavelength <= previous)
      throw std::runtime_error("Invalid Gaia XP spectral samples.");
    spectrum.push_back({wavelength, flux});
    previous = wavelength;
  }
  if (spectrum.size() < 3 || spectrum.front()[0] > 336 ||
      spectrum.back()[0] < 1020)
    throw std::runtime_error("Gaia XP does not cover 336–1020 nm.");
  return spectrum;
}
QMap<QString, processing::Spectrum>
parseGaiaSpectraArchive(const QByteArray &zip, const QStringList &ids) {
  if (zip.size() < 22 || zip.size() > 8 * 1024 * 1024 || ids.size() > 150)
    throw std::runtime_error("Invalid Gaia spectral archive size.");
  auto u16 = [&](int at) {
    if (at < 0 || at > zip.size() - 2)
      throw std::runtime_error("Truncated Gaia ZIP.");
    return qFromLittleEndian<quint16>(
        reinterpret_cast<const uchar *>(zip.constData() + at));
  };
  auto u32 = [&](int at) {
    if (at < 0 || at > zip.size() - 4)
      throw std::runtime_error("Truncated Gaia ZIP.");
    return qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(zip.constData() + at));
  };
  int end = -1;
  for (int at = zip.size() - 22; at >= std::max(0, int(zip.size()) - 65557);
       --at)
    if (u32(at) == 0x06054b50 && at + 22 + u16(at + 20) == zip.size()) {
      end = at;
      break;
    }
  if (end < 0 || u16(end + 4) || u16(end + 6) ||
      u16(end + 8) != u16(end + 10) || u16(end + 10) > 150)
    throw std::runtime_error("Unsupported Gaia ZIP directory.");
  const quint32 directory = u32(end + 16), directorySize = u32(end + 12);
  if (std::uint64_t(directory) + directorySize != std::uint64_t(end))
    throw std::runtime_error("Invalid Gaia ZIP bounds.");
  QMap<QString, processing::Spectrum> spectra;
  int at = int(directory);
  std::uint64_t total = 0;
  for (int n = 0; n < u16(end + 10); ++n) {
    if (u32(at) != 0x02014b50)
      throw std::runtime_error("Invalid Gaia ZIP central entry.");
    const auto flags = u16(at + 8), method = u16(at + 10);
    const auto crc = u32(at + 16), compressed = u32(at + 20),
               size = u32(at + 24), offset = u32(at + 42);
    const int nameLength = u16(at + 28), extra = u16(at + 30),
              comment = u16(at + 32);
    if (flags & 1 || u16(at + 34) || size > 256 * 1024 ||
        compressed > 256 * 1024 || (method != 0 && method != 8) ||
        at + 46 + nameLength + extra + comment > end)
      throw std::runtime_error("Unsupported or oversized Gaia ZIP entry.");
    at += 46 + nameLength + extra + comment;
    if (u32(int(offset)) != 0x04034b50 || u16(int(offset) + 8) != method)
      throw std::runtime_error("Invalid Gaia ZIP local entry.");
    const quint64 start =
        quint64(offset) + 30 + u16(int(offset) + 26) + u16(int(offset) + 28);
    if (start + compressed > directory || size == 0 ||
        total + size > 8 * 1024 * 1024)
      throw std::runtime_error("Invalid Gaia ZIP payload bounds.");
    total += size;
    QByteArray content;
    if (method == 0)
      content = zip.mid(int(start), compressed);
    else {
      content.resize(size);
      z_stream stream{};
      stream.next_in = reinterpret_cast<Bytef *>(
          const_cast<char *>(zip.constData() + start));
      stream.avail_in = compressed;
      stream.next_out = reinterpret_cast<Bytef *>(content.data());
      stream.avail_out = size;
      if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
        throw std::runtime_error("Cannot decode Gaia ZIP.");
      const int result = inflate(&stream, Z_FINISH);
      const bool complete = result == Z_STREAM_END &&
                            stream.total_out == size &&
                            stream.total_in == compressed;
      inflateEnd(&stream);
      if (!complete)
        throw std::runtime_error("Invalid Gaia ZIP deflate stream.");
    }
    if (content.size() != size ||
        crc32(0, reinterpret_cast<const Bytef *>(content.constData()),
              content.size()) != crc)
      throw std::runtime_error("Gaia ZIP checksum did not validate.");
    QXmlStreamReader xml(content);
    QString id;
    while (!xml.atEnd()) {
      xml.readNext();
      if (xml.isStartElement() && xml.name() == "PARAM" &&
          xml.attributes().value("name") == "source_id") {
        id = xml.attributes().value("value").toString();
        break;
      }
    }
    if (id.isEmpty() || !ids.contains(id) || spectra.contains(id))
      throw std::runtime_error("Unexpected or duplicate Gaia spectrum in ZIP.");
    spectra[id] = parseGaiaSpectrum(content, id);
  }
  if (at != end || spectra.size() != ids.size())
    throw std::runtime_error(
        "Gaia did not return every requested XP spectrum.");
  return spectra;
}
} // namespace astro::app
