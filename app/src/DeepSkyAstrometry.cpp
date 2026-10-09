#include "AstrometrySettings.hpp"
#include "DeepSkyController.hpp"
#include "GaiaCatalog.hpp"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScopedPointer>
#include <QStandardPaths>
#include <QTimeZone>
#include <QTimer>
#include <QUrlQuery>
#include <QXmlStreamReader>
#include <QtEndian>
#include <algorithm>
#include <bit>
#include <functional>
#include <future>
#include <set>
#include <stop_token>
#include <stdexcept>
namespace astro::app {
namespace {
void check(std::stop_token stop) {
  if (stop.stop_requested())
    throw std::runtime_error("Cancelled. Previous results are preserved.");
}
double number(QString text, double fallback = NAN) {
  bool ok = false;
  const double v = text.trimmed().toDouble(&ok);
  return ok && std::isfinite(v) ? v : fallback;
}
double epoch(QString date) {
  const auto dt = QDateTime::fromString(date, Qt::ISODate);
  const auto time = dt.isValid() ? dt : QDateTime::currentDateTimeUtc();
  return 2000 + (time.toMSecsSinceEpoch() -
                 QDateTime(QDate(2000, 1, 1), QTime(12, 0), QTimeZone::UTC)
                     .toMSecsSinceEpoch()) /
                    (365.25 * 86400000.);
}
QByteArray download(const QUrl &url, std::stop_token stop,
                    const std::function<void(const QByteArray &)> &validate) {
  check(stop);
  const auto directory =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      "/gaia-dr3";
  QDir().mkpath(directory);
  const auto path =
      directory + "/" +
      QString::fromLatin1(
          QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha256)
              .toHex());
  QFile cached(path);
  const bool immutable = url.path().contains("data-server");
  if (cached.exists() && cached.size() > 0 && cached.size() < 8 * 1024 * 1024 &&
      (immutable || QFileInfo(cached).lastModified().daysTo(
                        QDateTime::currentDateTimeUtc()) <= 7) &&
      cached.open(QIODevice::ReadOnly)) {
    const auto bytes = cached.readAll();
    cached.close();
    try {
      validate(bytes);
      return bytes;
    } catch (const std::exception &) {
      QFile::remove(path);
    }
  }
  thread_local QNetworkAccessManager network;
  QNetworkRequest request(url);
  request.setTransferTimeout(45000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  QScopedPointer<QNetworkReply> reply(network.get(request));
  QEventLoop loop;
  QTimer timer;
  timer.setInterval(100);
  bool oversize = false;
  int ticks = 0;
  QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
    if (stop.stop_requested() || ++ticks > 1200)
      reply->abort();
  });
  QObject::connect(reply.data(), &QNetworkReply::readyRead, &loop, [&] {
    if (reply->bytesAvailable() > 8 * 1024 * 1024) {
      oversize = true;
      reply->abort();
    }
  });
  QObject::connect(reply.data(), &QNetworkReply::finished, &loop,
                   &QEventLoop::quit);
  timer.start();
  loop.exec();
  check(stop);
  if (oversize)
    throw std::runtime_error(
        "Gaia response exceeds 8 MiB. Reduce the search field.");
  if (reply->error() != QNetworkReply::NoError) {
    qWarning() << "Gaia download failed" << reply->error()
               << reply->errorString()
               << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    throw std::runtime_error(
        immutable ? "Gaia XP spectra are unavailable. Check the connection and "
                    "retry. Previous results are preserved."
                  : "Gaia is unavailable. Check the connection or import a "
                    "local Gaia CSV. Previous results are preserved.");
  }
  const auto data = reply->readAll();
  if (data.isEmpty() || data.size() > 8 * 1024 * 1024)
    throw std::runtime_error("Invalid Gaia response.");
  validate(data);
  QSaveFile out(path);
  if (out.open(QIODevice::WriteOnly)) {
    out.write(data);
    out.commit();
  }
  auto files = QDir(directory).entryInfoList(QDir::Files, QDir::Time);
  qint64 total = 0;
  for (const auto &f : files) {
    total += f.size();
    if (total > 64 * 1024 * 1024)
      QFile::remove(f.filePath());
  }
  return data;
}
processing::SpectralResponse responseValues(const QVariantList &rows) {
  processing::SpectralResponse response;
  for (const auto &v : rows) {
    const auto a = v.toList();
    if (a.size() != 4)
      throw std::runtime_error("Invalid saved RGB response.");
    response.push_back(
        {a[0].toDouble(), {a[1].toDouble(), a[2].toDouble(), a[3].toDouble()}});
  }
  return response;
}
} // namespace
void DeepSkyController::importAstrometryCatalog(const QUrl &url) {
  if (busy_ || !url.isLocalFile())
    return;
  try {
    QFile f(url.toLocalFile());
    if (!f.open(QIODevice::ReadOnly) || f.size() > 8 * 1024 * 1024)
      throw std::runtime_error("Cannot read Gaia CSV (maximum 8 MiB).");
    const auto stars =
        parseGaiaCatalog(f.readAll(), epoch(astrometry_["date"].toString()));
    astrometry_["localCatalog"] = catalogValues(stars);
    astrometry_["localName"] = QFileInfo(f).fileName();
    for (const auto &[key, stack] : stacks_)
      if (stack == linear_)
        stackAstrometry_[key] = astrometry_;
    status_ = tr("Local Gaia catalogue imported: %1 stars.").arg(stars.size());
  } catch (const std::exception &e) {
    status_ = tr(e.what());
  }
  emit changed();
}
void DeepSkyController::importColorResponse(const QUrl &url) {
  if (busy_ || !url.isLocalFile())
    return;
  try {
    QFile f(url.toLocalFile());
    if (!f.open(QIODevice::ReadOnly) || f.size() > 256 * 1024)
      throw std::runtime_error(
          "Cannot read RGB response CSV (maximum 256 KiB).");
    const auto data = f.readAll();
    QVariantList rows;
    bool header = false;
    for (auto line : QString::fromUtf8(data).split('\n')) {
      line = line.trimmed();
      if (line.isEmpty() || line.startsWith('#'))
        continue;
      if (!header) {
        if (line != "wavelength_nm,red,green,blue")
          throw std::runtime_error(
              "Response CSV header must be wavelength_nm,red,green,blue.");
        header = true;
        continue;
      }
      const auto a = line.split(',');
      if (a.size() != 4 || rows.size() >= 2000)
        throw std::runtime_error("Invalid RGB response CSV.");
      QVariantList row;
      for (const auto &s : a)
        row << number(s);
      rows << QVariant::fromValue(row);
    }
    processing::spectralFlux(processing::solarSpectrum(), responseValues(rows));
    astrometry_["response"] = rows;
    astrometry_["responseName"] = QFileInfo(f).fileName();
    astrometry_["responseHash"] = QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    for (const auto &[key, stack] : stacks_)
      if (stack == linear_)
        stackAstrometry_[key] = astrometry_;
    status_ = tr("Equipment response imported. Include sensor QE × RGB filters "
                 "× any additional filter.");
  } catch (const std::exception &e) {
    status_ = tr(e.what());
  }
  emit changed();
}
void DeepSkyController::solveAstrometry(const QVariantMap &options) {
  if (busy_ || !linear_)
    return;
  const double ra = options["ra"].toDouble(), dec = options["dec"].toDouble(),
               scale = options["scale"].toDouble(),
               mag = options.value("magnitude", 16).toDouble();
  if (!std::isfinite(ra) || ra < 0 || ra >= 360 || !std::isfinite(dec) ||
      std::abs(dec) > 90 || !std::isfinite(scale) || scale < .01 ||
      scale > 100 || !std::isfinite(mag) || mag < 5 || mag > 20) {
    status_ = tr("Enter RA in degrees (0–360), declination (−90–90), sampling "
                 "(0.01–100 arcsec/pixel) and magnitude (5–20).");
    emit changed();
    return;
  }
  const auto image = linear_;
  const auto previous = astrometry_;
  const bool local = options["local"].toBool();
  run(tr("Solving astrometry with Gaia DR3…"), [this, image, previous, local,
                                                ra, dec, scale,
                                                mag](std::stop_token stop) {
    auto seed = readWcs(previous["wcs"])
                    .value_or(recording::CelestialWcs{
                        {ra, dec},
                        {(image->width + 1) / 2., (image->height + 1) / 2.},
                        {-scale / 3600, 0, 0, scale / 3600}});
    // Explicitly edited hints replace an existing header seed.
    if (std::abs(std::remainder(previous.value("ra", ra).toDouble() - ra,
                                360.)) > 1e-6 ||
        std::abs(previous.value("dec", dec).toDouble() - dec) > 1e-6 ||
        std::abs(previous.value("scale", scale).toDouble() - scale) > 1e-5)
      seed = {{ra, dec},
              {(image->width + 1) / 2., (image->height + 1) / 2.},
              {-scale / 3600, 0, 0, scale / 3600}};
    std::vector<processing::CatalogStar> catalog;
    if (local)
      catalog = readCatalogValues(previous["localCatalog"].toList());
    else {
      const double radius =
          std::hypot(image->width, image->height) * scale / 7200 * 1.25;
      if (radius > 8)
        throw std::runtime_error(
            "Targeted WCS supports a search radius up to 8 degrees. Use a "
            "local catalogue or narrower field.");
      const auto query =
          QString("SELECT TOP 5000 "
                  "source_id,ra,dec,pmra,pmdec,ref_epoch,phot_g_mean_mag,teff_"
                  "gspphot,has_xp_sampled FROM gaiadr3.gaia_source WHERE "
                  "1=CONTAINS(POINT('ICRS',ra,dec),CIRCLE('ICRS',%1,%2,%3)) "
                  "AND phot_g_mean_mag<%4 AND duplicated_source='false' ORDER "
                  "BY phot_g_mean_mag")
              .arg(ra, 0, 'g', 14)
              .arg(dec, 0, 'g', 14)
              .arg(radius, 0, 'g', 14)
              .arg(mag, 0, 'g', 8);
      QUrl url("https://gea.esac.esa.int/tap-server/tap/sync");
      QUrlQuery q;
      q.addQueryItem("REQUEST", "doQuery");
      q.addQueryItem("LANG", "ADQL");
      q.addQueryItem("FORMAT", "csv");
      q.addQueryItem("QUERY", query);
      url.setQuery(q);
      const auto observationEpoch = epoch(previous["date"].toString());
      const auto bytes = download(url, stop, [&](const auto &data) {
        parseGaiaCatalog(data, observationEpoch);
      });
      catalog = parseGaiaCatalog(bytes, observationEpoch);
    }
    catalog.erase(
        std::remove_if(catalog.begin(), catalog.end(),
                       [&](const auto &s) { return s.magnitude > mag; }),
        catalog.end());
    post([this] {
      stage_ = tr("Matching stars and verifying WCS…");
      progress_ = 50;
      emit changed();
    });
    const auto solution = processing::solveAstrometry(*image, catalog, seed,
                                                      [stop] { check(stop); });
    const auto centre =
        solution.wcs.sky((image->width - 1) / 2., (image->height - 1) / 2.);
    auto state = previous;
    state["wcs"] = wcsValues(solution.wcs);
    state["verified"] = true;
    state["ra"] = (*centre)[0];
    state["dec"] = (*centre)[1];
    state["scale"] = solution.wcs.scale();
    state["rms"] = solution.rms;
    state["coverage"] = solution.coverage;
    state["matches"] = int(solution.matches.size());
    state["catalog"] = catalogValues(catalog);
    state["imageKey"] = QString::fromLatin1(linearImageKey(*image));
    state["catalogName"] =
        local ? previous["localName"] : QVariant("Gaia DR3 · ESA");
    state["epoch"] = epoch(previous["date"].toString());
    QVariantList positions;
    for (const auto &m : solution.matches)
      positions << QVariant::fromValue(
          QVariantList{m.x / image->width, m.y / image->height, m.residual});
    state["positions"] = positions;
    check(stop);
    post([this, image, state, stop] {
      if (stop.stop_requested() || linear_ != image)
        return;
      astrometry_ = state;
      for (const auto &[key, stack] : stacks_)
        if (stack == image)
          stackAstrometry_[key] = state;
      progress_ = 100;
      status_ = tr("WCS verified: %1 stars · RMS %2 px · %3 arcsec/pixel.")
                    .arg(state["matches"].toInt())
                    .arg(state["rms"].toDouble(), 0, 'f', 3)
                    .arg(state["scale"].toDouble(), 0, 'f', 3);
      emit changed();
    });
  });
}
void DeepSkyController::prepareCatalogCalibration(int method,
                                                  const QVariantMap &options) {
  if (busy_ || !linear_)
    return;
  if ((method != 0 && method != 1) || !astrometry_["verified"].toBool() ||
      !readWcs(astrometry_["wcs"])) {
    status_ = tr("Solve and verify WCS before calibrating colors.");
    emit changed();
    return;
  }
  const auto image = linear_;
  const auto state = astrometry_;
  const bool custom = options["customResponse"].toBool();
  const int white = options["white"].toInt();
  bool validLimit = false;
  const double limitValue =
      options.value("maxStars", method ? 60 : 150).toDouble(&validLimit);
  if (!validLimit || !std::isfinite(limitValue) || limitValue < 8 ||
      limitValue > 150 || limitValue != std::floor(limitValue)) {
    status_ = tr("Choose a reference limit between 8 and 150 stars.");
    emit changed();
    return;
  }
  const int referenceLimit = int(limitValue);
  if ((method == 1 && !custom) ||
      (custom && state["response"].toList().isEmpty())) {
    status_ = tr("SPCC requires the measured RGB response of your equipment. "
                 "Import its CSV first.");
    emit changed();
    return;
  }
  run(method ? tr("Preparing SPCC from Gaia XP spectra…")
             : tr("Preparing PCC from Gaia stellar temperatures…"),
      [this, image, state, method, custom, white,
       referenceLimit](std::stop_token stop) {
        if (QString::fromLatin1(linearImageKey(*image)) !=
            state["imageKey"].toString())
          throw std::runtime_error("The WCS belongs to a different linear "
                                   "image. Solve this image again.");
        const auto response = custom
                                  ? responseValues(state["response"].toList())
                                  : processing::standardResponse();
        auto neutral = processing::solarSpectrum();
        if (white == 1) {
          neutral.clear();
          for (int w = 336; w <= 1020; w += 2)
            neutral.push_back({double(w), 1. / (double(w) * w)});
        } else if (white != 0)
          throw std::runtime_error("Invalid white reference.");
        const auto catalog = readCatalogValues(state["catalog"].toList());
        const auto wcs = *readWcs(state["wcs"]);
        const auto stars =
            processing::detectStars(*image, [stop] { check(stop); }, 1500);
        QVariantList references;
        int missing = 0;
        std::set<int> used;
        struct Reference {
          processing::CatalogStar source;
          processing::Star measured;
        };
        std::vector<Reference> selected;
        for (const auto &star : catalog) {
          check(stop);
          if (selected.size() >= referenceLimit)
            break;
          if (method ? !star.spectrum
                     : (star.temperature < 2500 || star.temperature > 50000)) {
            ++missing;
            continue;
          }
          const auto p = wcs.pixel(star.ra, star.dec);
          if (!p || (*p)[0] < 12 || (*p)[1] < 12 ||
              (*p)[0] >= image->width - 12 || (*p)[1] >= image->height - 12)
            continue;
          int closest = -1;
          double distance = 2.5;
          for (int n = 0; n < int(stars.size()); ++n)
            if (!used.contains(n)) {
              const double d =
                  std::hypot(stars[n].x - (*p)[0], stars[n].y - (*p)[1]);
              if (d < distance) {
                distance = d;
                closest = n;
              }
            }
          if (closest < 0) {
            ++missing;
            continue;
          }
          used.insert(closest);
          selected.push_back({star, stars[closest]});
        }
        QMap<QString, processing::Spectrum> spectra;
        if (selected.size() < 8)
          throw std::runtime_error(
              "Fewer than eight identified color references are usable. Check "
              "WCS, field, passbands and catalogue coverage.");
        if (method) {
          std::stop_source retrievalStop;
          std::stop_callback cancelRequests(
              stop, [&] { retrievalStop.request_stop(); });
          post([this, total = selected.size()] {
            stage_ =
                tr("Downloading Gaia XP spectra: %1 / %2").arg(0).arg(total);
            emit changed();
          });
          // Keep requests small: ESA streams each spectrum into its ZIP. Three
          // bounded requests run at once; cancellation reaches every request.
          for (std::size_t wave = 0; wave < selected.size(); wave += 30) {
            std::vector<std::future<QMap<QString, processing::Spectrum>>>
                requests;
            for (std::size_t start = wave;
                 start < std::min(wave + 30, selected.size()); start += 10) {
              QStringList ids;
              for (std::size_t n = start;
                   n < std::min(start + 10, selected.size()); ++n)
                ids << QString::fromStdString(selected[n].source.id);
              requests.push_back(std::async(
                  std::launch::async,
                  [ids, requestStop = retrievalStop.get_token()] {
                    QUrl url("https://gea.esac.esa.int/data-server/data");
                    QUrlQuery q;
                    q.addQueryItem("ID", ids.join(','));
                    q.addQueryItem("RETRIEVAL_TYPE", "XP_SAMPLED");
                    q.addQueryItem("RELEASE", "Gaia DR3");
                    q.addQueryItem("FORMAT", "VOTABLE");
                    url.setQuery(q);
                    const auto bytes =
                        download(url, requestStop, [&](const auto &data) {
                          parseGaiaSpectraArchive(data, ids);
                        });
                    return parseGaiaSpectraArchive(bytes, ids);
                  }));
            }
            for (auto &request : requests) {
              QMap<QString, processing::Spectrum> batch;
              try {
                batch = request.get();
              } catch (...) {
                retrievalStop.request_stop();
                throw;
              }
              for (auto i = batch.cbegin(); i != batch.cend(); ++i)
                spectra.insert(i.key(), i.value());
              check(stop);
              post([this, done = spectra.size(), total = selected.size()] {
                progress_ = int(done * 90 / std::max(std::size_t(1), total));
                stage_ = tr("Downloading Gaia XP spectra: %1 / %2")
                             .arg(done)
                             .arg(total);
                emit changed();
              });
            }
          }
        }
        for (const auto &star : selected) {
          check(stop);
          const auto spectrum =
              method ? spectra[QString::fromStdString(star.source.id)]
                     : processing::temperatureSpectrum(star.source.temperature);
          const auto f =
              processing::referenceColors(spectrum, response, neutral);
          references << QVariant::fromValue(
              QVariantList{star.measured.x, star.measured.y, f[0], f[1], f[2]});
        }
        if (references.size() < 8)
          throw std::runtime_error(
              "Fewer than eight identified color references are usable. Check "
              "WCS, field, passbands and catalogue coverage.");
        QVariantMap prepared{
            {"red", 1.0},
            {"green", 1.0},
            {"blue", 1.0},
            {"photometry", true},
            {"photometryReferences", references},
            {"photometryMethod",
             method ? "SPCC · Gaia DR3 XP" : "PCC · Gaia DR3 Teff"},
            {"photometryProfile", custom ? state["responseName"].toString() +
                                               " · SHA256 " +
                                               state["responseHash"].toString()
                                         : "Bessell R/V/B · continuum model"},
            {"photometryWhite",
             white ? "Flat Fν (AB)" : "Sun · CALSPEC STIS 002"},
            {"photometryImageKey", state["imageKey"]}};
        check(stop);
        post([this, image, prepared, missing, stop, referenceLimit] {
          if (stop.stop_requested() || linear_ != image)
            return;
          astrometry_["referenceLimit"] = referenceLimit;
          for (const auto &[key, stack] : stacks_)
            if (stack == image)
              stackAstrometry_[key] = astrometry_;
          progress_ = 100;
          status_ = tr("%1 color references prepared; %2 unavailable or "
                       "unmatched. Apply on linear data before stretching.")
                        .arg(prepared["photometryReferences"].toList().size())
                        .arg(missing);
          emit catalogCalibrationReady(prepared);
          emit changed();
        });
      });
}
} // namespace astro::app
