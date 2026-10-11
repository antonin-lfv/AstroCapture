#include "TargetResolver.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QXmlStreamReader>
#include <cmath>
#include <stdexcept>

namespace astro::app {
namespace {
void check(std::stop_token stop) {
  if (stop.stop_requested())
    throw std::runtime_error("Cancelled. Previous results are preserved.");
}
QString targetName(QString name) {
  name = name.simplified();
  if (name.isEmpty() || name.size() > 120)
    throw std::runtime_error(
        "Enter an astronomical target name (maximum 120 characters).");
  for (auto c : name)
    if (c.category() == QChar::Other_Control)
      throw std::runtime_error("Invalid target name.");
  return name;
}
} // namespace
QUrl targetResolverUrl(const QString &name) {
  return QUrl::fromEncoded(
      "https://cds.unistra.fr/cgi-bin/nph-sesame/-ox/SNV?" +
      QUrl::toPercentEncoding(targetName(name)));
}
QVariantMap parseTargetPosition(const QByteArray &bytes, const QString &query) {
  const auto requested = targetName(query);
  if (bytes.isEmpty() || bytes.size() > 64 * 1024)
    throw std::runtime_error("Invalid target resolver response.");
  QXmlStreamReader xml(bytes);
  int targets = 0, solutions = 0, raCount = 0, decCount = 0;
  bool inTarget = false, inResolver = false, root = false;
  double ra = NAN, dec = NAN, candidateRa = NAN, candidateDec = NAN;
  QString name, echo, source, candidateName, candidateSource;
  while (!xml.atEnd()) {
    xml.readNext();
    if (xml.isDTD() || xml.isEntityReference())
      throw std::runtime_error("Invalid target resolver response.");
    if (xml.isStartElement()) {
      const auto tag = xml.name();
      if (!root && tag != u"Sesame")
        throw std::runtime_error("Invalid target resolver response.");
      if (tag == u"Sesame") {
        if (root)
          throw std::runtime_error("Invalid target resolver response.");
        root = true;
      } else if (tag == u"Target") {
        ++targets;
        inTarget = true;
      } else if (tag == u"Resolver" && inTarget) {
        if (inResolver)
          throw std::runtime_error("Invalid target resolver response.");
        inResolver = true;
        raCount = decCount = 0;
        candidateRa = candidateDec = NAN;
        candidateName.clear();
        candidateSource = xml.attributes().value("name").toString().left(512);
      } else if (tag == u"name" && inTarget && !inResolver)
        echo = xml.readElementText().simplified();
      else if (tag == u"oname" && inResolver)
        candidateName = xml.readElementText().simplified();
      else if ((tag == u"jradeg" || tag == u"jdedeg") && inResolver) {
        const bool isRa = tag == u"jradeg";
        bool ok = false;
        const double value = xml.readElementText().trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value))
          throw std::runtime_error("Invalid target resolver coordinates.");
        if (isRa) {
          candidateRa = value;
          ++raCount;
        } else {
          candidateDec = value;
          ++decCount;
        }
      }
    } else if (xml.isEndElement()) {
      if (xml.name() == u"Resolver") {
        inResolver = false;
        if (raCount || decCount) {
          if (raCount != 1 || decCount != 1 || candidateRa < 0 ||
              candidateRa >= 360 || std::abs(candidateDec) > 90 ||
              candidateName.size() > 512 || candidateSource.isEmpty())
            throw std::runtime_error("Invalid target resolver coordinates.");
          ++solutions;
          ra = candidateRa;
          dec = candidateDec;
          name = candidateName;
          source = candidateSource;
        }
      }
      if (xml.name() == u"Target")
        inTarget = false;
    }
  }
  if (xml.hasError() || !root || targets != 1 ||
      echo.compare(requested, Qt::CaseInsensitive) != 0)
    throw std::runtime_error("Invalid target resolver response.");
  if (solutions == 0)
    throw std::runtime_error("Target not found. Try its catalogue designation, "
                             "such as M42 or NGC 7000.");
  if (solutions != 1)
    throw std::runtime_error(
        "Target name is ambiguous. Use a more precise catalogue designation.");
  QString displaySource = "CDS Sesame";
  if (source.contains("Simbad", Qt::CaseInsensitive))
    displaySource = "SIMBAD · CDS";
  else if (source.contains("NED", Qt::CaseInsensitive))
    displaySource = "NED";
  else if (source.contains("VizieR", Qt::CaseInsensitive))
    displaySource = "VizieR · CDS";
  return {{"query", requested},
          {"name", name.isEmpty() ? requested : name},
          {"ra", ra},
          {"dec", dec},
          {"source", displaySource},
          {"resolver", source}};
}
QVariantMap resolveTargetPosition(const QString &name, std::stop_token stop) {
  const auto url = targetResolverUrl(name);
  check(stop);
  const auto directory =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      "/cds-sesame";
  QDir().mkpath(directory);
  const auto path =
      directory + "/" +
      QString::fromLatin1(
          QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256)
              .toHex());
  QFile cache(path);
  if (cache.size() > 0 && cache.size() <= 64 * 1024 &&
      QFileInfo(cache).lastModified().daysTo(QDateTime::currentDateTimeUtc()) <=
          30 &&
      cache.open(QIODevice::ReadOnly)) {
    const auto data = cache.readAll();
    cache.close();
    try {
      return parseTargetPosition(data, name);
    } catch (const std::exception &) {
      QFile::remove(path);
    }
  }
  QNetworkAccessManager network;
  QNetworkRequest request(url);
  request.setTransferTimeout(15000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = network.get(request);
  QEventLoop loop;
  QTimer timer;
  timer.setInterval(100);
  int ticks = 0;
  bool oversized = false;
  QObject::connect(reply, &QNetworkReply::readyRead, &loop, [&] {
    if (reply->bytesAvailable() > 64 * 1024) {
      oversized = true;
      reply->abort();
    }
  });
  QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
    if (stop.stop_requested() || ++ticks > 300)
      reply->abort();
  });
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  timer.start();
  loop.exec();
  check(stop);
  if (oversized)
    throw std::runtime_error("Invalid target resolver response.");
  if (reply->error() != QNetworkReply::NoError)
    throw TargetResolverNetworkError(int(reply->error()));
  const auto data = reply->readAll();
  const auto result = parseTargetPosition(data, name);
  check(stop);
  QSaveFile out(path);
  if (out.open(QIODevice::WriteOnly)) {
    out.write(data);
    out.commit();
  }
  qint64 total = 0;
  for (const auto &file :
       QDir(directory).entryInfoList(QDir::Files, QDir::Time)) {
    total += file.size();
    if (total > 1024 * 1024)
      QFile::remove(file.filePath());
  }
  return result;
}
QString rightAscensionText(double degrees) {
  if (!std::isfinite(degrees) || degrees < 0 || degrees >= 360)
    return {};
  const qint64 ticks =
      qint64(std::llround(degrees / 15 * 360000)) % (24 * 360000);
  return QString("%1:%2:%3.%4")
      .arg(ticks / 360000, 2, 10, QChar('0'))
      .arg(ticks / 6000 % 60, 2, 10, QChar('0'))
      .arg(ticks / 100 % 60, 2, 10, QChar('0'))
      .arg(ticks % 100, 2, 10, QChar('0'));
}
QString declinationText(double degrees) {
  if (!std::isfinite(degrees) || std::abs(degrees) > 90)
    return {};
  const qint64 ticks = qint64(std::llround(std::abs(degrees) * 36000));
  return QString("%1%2:%3:%4.%5")
      .arg(std::signbit(degrees) ? "-" : "+")
      .arg(ticks / 36000, 2, 10, QChar('0'))
      .arg(ticks / 600 % 60, 2, 10, QChar('0'))
      .arg(ticks / 10 % 60, 2, 10, QChar('0'))
      .arg(ticks % 10);
}
} // namespace astro::app
