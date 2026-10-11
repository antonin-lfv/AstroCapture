#include "ObservationController.hpp"
#include "TargetResolver.hpp"
#include "astronomy.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSettings>
#include <QTimeZone>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::app {
namespace {
QString normalized(QString s) {
  s = s.toCaseFolded().normalized(QString::NormalizationForm_D);
  s.remove(QRegularExpression("[\\s\\p{M}_-]"));
  return s;
}
QString designation(QString s) {
  auto m = QRegularExpression("^(NGC|IC|M|B|C)(0*)([0-9]+)(.*)$").match(s);
  return m.hasMatch() ? m.captured(1) + m.captured(3) + m.captured(4) : s;
}
QStringList fields(const QString &line) {
  QStringList out;
  QString field;
  bool quote = false;
  for (int i = 0; i < line.size(); ++i) {
    auto c = line[i];
    if (c == '"') {
      if (quote && i + 1 < line.size() && line[i + 1] == '"') {
        field += '"';
        ++i;
      } else
        quote = !quote;
    } else if (c == ';' && !quote) {
      out << field;
      field.clear();
    } else
      field += c;
  }
  out << field;
  return out;
}
double angle(QString text, bool hours) {
  const bool negative = text.startsWith('-');
  text.remove('+');
  text.remove('-');
  auto p = text.split(':');
  if (p.size() != 3)
    return NAN;
  bool a, b, c;
  double v =
      p[0].toDouble(&a) + p[1].toDouble(&b) / 60 + p[2].toDouble(&c) / 3600;
  if (!a || !b || !c)
    return NAN;
  return v * (negative ? -1 : 1) * (hours ? 15 : 1);
}
QVariantMap site(QString name, double lat, double lon, QString zone) {
  return {{"name", name},   {"latitude", lat}, {"longitude", lon},
          {"elevation", 0}, {"zone", zone},    {"favorite", false}};
}
} // namespace
ObservationController::ObservationController(QObject *parent)
    : QObject(parent) {
  loadCatalog();
  sites_ = {
      site(tr("Paris"), 48.8566, 2.3522, "Europe/Paris"),
      site(tr("Lyon"), 45.764, 4.8357, "Europe/Paris"),
      site(tr("Marseille"), 43.2965, 5.3698, "Europe/Paris"),
      site(tr("La Serena · Chile"), -29.9027, -71.2519, "America/Santiago"),
      site(tr("Sydney"), -33.8688, 151.2093, "Australia/Sydney"),
      site(tr("Tokyo"), 35.6762, 139.6503, "Asia/Tokyo"),
      site(tr("New York"), 40.7128, -74.006, "America/New_York"),
      site(tr("Tromsø"), 69.6492, 18.9553, "Europe/Oslo"),
      site(tr("Cape Town"), -33.9249, 18.4241, "Africa/Johannesburg")};
  const auto stored = QSettings().value("observation/favorites").toList();
  for (const auto &value : stored) {
    auto s = value.toMap();
    if (!QTimeZone(s.value("zone").toString().toUtf8()).isValid() ||
        !std::isfinite(s.value("latitude").toDouble()) ||
        std::abs(s.value("latitude").toDouble()) > 90 ||
        std::abs(s.value("longitude").toDouble()) > 180)
      continue;
    s["favorite"] = true;
    sites_.append(s);
  }
  settings_ = QSettings().value("observation/settings").toMap();
  if (settings_.isEmpty())
    settings_ = sites_[0].toMap();
  settings_["date"] = QDate::currentDate().toString(Qt::ISODate);
  settings_["minimum"] = settings_.value("minimum", 30);
  settings_["sunLimit"] = settings_.value("sunLimit", -18);
  target_ = catalog_[0];
  const QString targetId = settings_.value("target", "NGC1976").toString();
  for (const auto &t : catalog_)
    if (t.id == targetId) {
      target_ = t;
      break;
    }
  if (targetId == "manual")
    target_ = {
        "manual",
        settings_.value("targetName", tr("Manual coordinates")).toString(),
        tr("Manual J2000 coordinates"),
        -1,
        settings_.value("ra").toDouble(),
        settings_.value("dec").toDouble()};
  calculate(settings_);
  search("");
}
ObservationController::~ObservationController() {
  resolver_.request_stop();
  if (resolver_.joinable())
    resolver_.join();
}
void ObservationController::loadCatalog() {
  catalog_ = {{"moon", tr("Moon"), "Astronomy Engine", BODY_MOON},
              {"mercury", tr("Mercury"), "Astronomy Engine", BODY_MERCURY},
              {"venus", tr("Venus"), "Astronomy Engine", BODY_VENUS},
              {"mars", tr("Mars"), "Astronomy Engine", BODY_MARS},
              {"jupiter", tr("Jupiter"), "Astronomy Engine", BODY_JUPITER},
              {"saturn", tr("Saturn"), "Astronomy Engine", BODY_SATURN},
              {"uranus", tr("Uranus"), "Astronomy Engine", BODY_URANUS},
              {"neptune", tr("Neptune"), "Astronomy Engine", BODY_NEPTUNE}};
  for (const QString path : {QString(":/observation/NGC.csv"),
                             QString(":/observation/NGC-addendum.csv")}) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
      continue;
    file.readLine();
    while (!file.atEnd()) {
      auto p = fields(QString::fromUtf8(file.readLine()).trimmed());
      if (p.size() < 29 || p[1] == "Dup" || p[1] == "NonEx")
        continue;
      auto ra = angle(p[2], true), dec = angle(p[3], false);
      if (!std::isfinite(ra) || !std::isfinite(dec))
        continue;
      QString name = designation(p[0]);
      if (!p[23].isEmpty())
        name = "M" + QString::number(p[23].toInt()) + " · " + name;
      if (p[0] == "NGC1976")
        name += tr(" · Orion Nebula");
      else if (p[0] == "NGC0224")
        name += tr(" · Andromeda Galaxy");
      else if (p[0] == "M045")
        name += tr(" · Pleiades");
      else if (!p[28].isEmpty())
        name += " · " + p[28].split(',')[0];
      catalog_.push_back({p[0], name, "OpenNGC · J2000", -1, ra, dec});
    }
  }
}
void ObservationController::search(const QString &query) {
  ++searchGeneration_;
  searchResults_.clear();
  searchStatus_.clear();
  const auto q = normalized(query);
  std::vector<ObservationTarget> matching;
  const QStringList suggested = {"moon", "jupiter", "saturn",
                                 "mars", "NGC1976", "NGC0224",
                                 "M045", "NGC6205", "NGC3372"};
  for (const auto &t : catalog_)
    if ((q.isEmpty() && suggested.contains(t.id)) ||
        (!q.isEmpty() &&
         (normalized(t.name).contains(q) ||
          normalized(designation(t.id)) == q || normalized(t.id) == q)))
      matching.push_back(t);
  std::stable_sort(matching.begin(), matching.end(),
                   [&](const auto &a, const auto &b) {
                     auto rank = [&](const auto &t) {
                       if (normalized(designation(t.id)) == q)
                         return 0;
                       auto first = normalized(t.name.split(" · ")[0]);
                       return first == q ? 0 : t.body >= 0 ? 1 : 2;
                     };
                     return rank(a) < rank(b);
                   });
  for (const auto &t : matching) {
    if (searchResults_.size() == 80)
      break;
    searchResults_.append(QVariantMap{{"id", t.id},
                                      {"name", t.name},
                                      {"source", t.source},
                                      {"moving", t.body >= 0}});
  }
  if (matching.size() > 80)
    searchStatus_ = tr("80 results shown. Refine the name.");
  emit searchChanged();
}
void ObservationController::searchOnline(const QString &query) {
  if (searching_)
    return;
  searching_ = true;
  searchStatus_ = tr("Searching CDS Sesame…");
  emit searchChanged();
  const int generation = ++searchGeneration_;
  resolver_ = std::jthread([this, query, generation](std::stop_token stop) {
    QVariantMap answer;
    QString error;
    try {
      answer = resolveTargetPosition(query, stop);
    } catch (const std::exception &e) {
      error = QString::fromUtf8(e.what());
    }
    if (stop.stop_requested())
      return;
    QMetaObject::invokeMethod(
        this,
        [this, answer, error, generation] {
          searching_ = false;
          if (generation == searchGeneration_) {
            if (error.isEmpty()) {
              const QString id = "cds:" + answer.value("query").toString();
              catalog_.push_back({id, answer.value("name").toString(),
                                  answer.value("source").toString(), -1,
                                  answer.value("ra").toDouble(),
                                  answer.value("dec").toDouble()});
              searchResults_ = {QVariantMap{{"id", id},
                                            {"name", answer.value("name")},
                                            {"source", answer.value("source")},
                                            {"moving", false}}};
              searchStatus_ = tr("Object found. Select it below.");
            } else
              searchStatus_ =
                  tr("Search unavailable or name not found. Try a catalogue "
                     "identifier, or enter coordinates in Advanced.");
          }
          emit searchChanged();
        },
        Qt::QueuedConnection);
  });
}
bool ObservationController::selectTarget(const QString &id) {
  for (const auto &t : catalog_)
    if (t.id == id) {
      auto previous = target_;
      target_ = t;
      auto options = settings_;
      options["sunLimit"] = t.body >= 0 ? -6 : -18;
      if (calculate(options))
        return true;
      target_ = previous;
      return false;
    }
  return false;
}
QString ObservationController::localTime(qint64 utc) const {
  return QDateTime::fromSecsSinceEpoch(
             utc, QTimeZone(settings_.value("zone").toString().toUtf8()))
      .toString("dd/MM HH:mm t");
}
void ObservationController::persist() {
  QSettings prefs;
  auto s = settings_;
  s["target"] = target_.id.startsWith("cds:") ? "manual" : target_.id;
  s["targetName"] = target_.name;
  s["ra"] = target_.ra;
  s["dec"] = target_.dec;
  prefs.setValue("observation/settings", s);
}
bool ObservationController::calculate(const QVariantMap &options) {
  try {
    const auto start = observationNoon(options.value("date").toString(),
                                       options.value("zone").toString());
    const auto end =
        QDateTime(start.date().addDays(1), QTime(12, 0), start.timeZone());
    auto number = [&](const char *key) {
      bool ok;
      auto raw = options.value(key);
      double value =
          raw.typeId() == QMetaType::QString
              ? QString(raw.toString()).replace(',', '.').toDouble(&ok)
              : raw.toDouble(&ok);
      if (!ok || !std::isfinite(value))
        throw std::runtime_error("Enter valid numerical values.");
      return value;
    };
    ObservationSite s{number("latitude"), number("longitude"),
                      number("elevation")};
    const double minimum = number("minimum"), sunLimit = number("sunLimit");
    ObservationTarget t = target_;
    if (options.value("manual").toBool())
      t = {"manual",
           tr("Manual coordinates"),
           tr("Manual J2000 coordinates"),
           -1,
           number("ra"),
           number("dec")};
    const auto samples = observationSamples(t, s, start.toSecsSinceEpoch(),
                                            end.toSecsSinceEpoch());
    const auto windows =
        observationWindows(t, s, start.toSecsSinceEpoch(),
                           end.toSecsSinceEpoch(), minimum, sunLimit);
    target_ = t;
    site_ = s;
    settings_ = options;
    settings_.remove("manual");
    settings_["target"] = t.id;
    settings_["targetName"] = t.name;
    settings_["ra"] = t.ra;
    settings_["dec"] = t.dec;
    start_ = start.toSecsSinceEpoch();
    end_ = end.toSecsSinceEpoch();
    QVariantList list, windowsList, events;
    double lo = 90, hi = -90, sunLo = 90;
    int peak = 0;
    for (int i = 0; i < int(samples.size()); ++i) {
      const auto &sample = samples[i];
      list.append(QVariantMap{
          {"fraction", double(sample.utc - start_) / (end_ - start_)},
          {"altitude", sample.altitude},
          {"azimuth", sample.azimuth},
          {"sun", sample.sun},
          {"time", QDateTime::fromSecsSinceEpoch(sample.utc, start.timeZone())
                       .toString("HH:mm")}});
      lo = std::min(lo, sample.altitude);
      sunLo = std::min(sunLo, sample.sun);
      if (sample.altitude > hi) {
        hi = sample.altitude;
        peak = i;
      }
      if (i > 0 && ((samples[i - 1].altitude >= 0) != (sample.altitude >= 0))) {
        qint64 a = samples[i - 1].utc, b = sample.utc;
        const bool up = sample.altitude >= 0;
        while (b - a > 1) {
          auto mid = (a + b) / 2;
          if ((observationPosition(t, s, mid).altitude >= 0) == up)
            b = mid;
          else
            a = mid;
        }
        events.append(
            QVariantMap{{"name", up ? tr("Rise (centre)") : tr("Set (centre)")},
                        {"time", localTime(b)}});
      }
    }
    qint64 a = samples[std::max(0, peak - 1)].utc,
           b = samples[std::min(int(samples.size()) - 1, peak + 1)].utc;
    while (b - a > 2) {
      auto x = a + (b - a) / 3, y = b - (b - a) / 3;
      if (observationPosition(t, s, x).altitude <
          observationPosition(t, s, y).altitude)
        a = x;
      else
        b = y;
    }
    auto highest = (a + b) / 2;
    auto highestAlt = observationPosition(t, s, highest).altitude;
    for (auto w : windows)
      windowsList.append(QVariantMap{
          {"start", localTime(w.start)},
          {"end", localTime(w.end)},
          {"startFraction", double(w.start - start_) / (end_ - start_)},
          {"endFraction", double(w.end - start_) / (end_ - start_)},
          {"minutes", (w.end - w.start) / 60}});
    result_ = {
        {"target", t.name},
        {"source", t.source},
        {"moving", t.body >= 0},
        {"moonTarget", t.body == BODY_MOON},
        {"peakTime", localTime(highest)},
        {"peakAltitude", highestAlt},
        {"slots", windowsList},
        {"events", events},
        {"period", lo >= 0  ? tr("Above the horizon throughout this period")
                   : hi < 0 ? tr("Below the horizon throughout this period")
                            : tr("Rises and sets during this period")},
        {"noDarkness", sunLo > sunLimit},
        {"noAstronomicalNight", sunLo > -18},
        {"startUtc", start.toUTC().toString("dd/MM/yyyy HH:mm 'UTC'")},
        {"endUtc", end.toUTC().toString("dd/MM/yyyy HH:mm 'UTC'")},
        {"durationHours", (end_ - start_) / 3600.0}};
    samples_ = list;
    error_.clear();
    persist();
    updateInstant();
    emit changed();
    return true;
  } catch (const std::exception &e) {
    error_ = tr(e.what());
    emit changed();
    return false;
  }
}
void ObservationController::setFraction(double value) {
  if (!std::isfinite(value) || !start_)
    return;
  fraction_ = std::clamp(value, 0.0, 1.0);
  updateInstant();
}
void ObservationController::updateInstant() {
  const qint64 utc = start_ + qRound64(fraction_ * (end_ - start_));
  const auto p = observationPosition(target_, site_, utc),
             moon = observationPosition({{}, {}, {}, BODY_MOON}, site_, utc);
  const double sun = observationSunAltitude(site_, utc),
               minimum = settings_["minimum"].toDouble(),
               limit = settings_["sunLimit"].toDouble();
  QString status = p.altitude < 0         ? tr("Below the horizon")
                   : p.altitude < minimum ? tr("Too low")
                   : sun > limit
                       ? tr("Sky too bright for the chosen limit")
                       : tr("Above the threshold, in the chosen darkness");
  instant_ = {{"fraction", fraction_},
              {"utcSeconds", utc},
              {"local", localTime(utc)},
              {"utc", QDateTime::fromSecsSinceEpoch(utc, QTimeZone::UTC)
                          .toString("dd/MM/yyyy HH:mm 'UTC'")},
              {"altitude", p.altitude},
              {"azimuth", p.azimuth},
              {"sun", sun},
              {"moonAltitude", moon.altitude},
              {"moonFraction", observationMoonFraction(utc) * 100},
              {"moonSeparation", observationSeparation(p, moon)},
              {"status", status}};
  // Topocentric calculations on a 6-degree grid. This is a regional overview,
  // not a site forecast.
  world_.clear();
  for (int lat = -87; lat < 90; lat += 6)
    for (int lon = -177; lon < 180; lon += 6) {
      ObservationSite s{double(lat), double(lon), 0};
      const double alt = observationPosition(target_, s, utc).altitude,
                   solar = observationSunAltitude(s, utc);
      int state = alt < 0 ? 0 : alt < minimum ? 1 : solar > limit ? 2 : 3;
      world_.append(QVariantMap{{"lat", lat}, {"lon", lon}, {"state", state}});
    }
  emit instantChanged();
}
void ObservationController::chooseMapLocation(double lat, double lon) {
  auto s = settings_;
  s["name"] = tr("Map position");
  s["latitude"] = lat;
  s["longitude"] = lon;
  s["elevation"] = 0;
  s["zone"] = "UTC";
  const auto utc = instant_.value("utcSeconds").toLongLong();
  const auto d = QDateTime::fromSecsSinceEpoch(utc, QTimeZone::UTC);
  s["date"] = (d.time().hour() < 12 ? d.date().addDays(-1) : d.date())
                  .toString(Qt::ISODate);
  if (calculate(s))
    setFraction(double(utc - start_) / (end_ - start_));
}
bool ObservationController::saveSite(const QString &name) {
  if (name.trimmed().isEmpty() || name.size() > 100 || sites_.size() >= 39)
    return false;
  auto s = settings_;
  s.remove("date");
  s.remove("target");
  s.remove("ra");
  s.remove("dec");
  s["name"] = name.trimmed();
  s["favorite"] = true;
  sites_.append(s);
  QVariantList favorites;
  for (const auto &value : sites_)
    if (value.toMap().value("favorite").toBool())
      favorites.append(value);
  QSettings().setValue("observation/favorites", favorites);
  emit sitesChanged();
  return true;
}
void ObservationController::removeSite(int index) {
  if (index < 0 || index >= sites_.size() ||
      !sites_[index].toMap().value("favorite").toBool())
    return;
  sites_.removeAt(index);
  QVariantList favorites;
  for (const auto &value : sites_)
    if (value.toMap().value("favorite").toBool())
      favorites.append(value);
  QSettings().setValue("observation/favorites", favorites);
  emit sitesChanged();
}
void ObservationController::searchCity(const QString &name) {
  if (locating_ || name.trimmed().size() < 2 || name.size() > 100)
    return;
  locating_ = true;
  cityResults_.clear();
  locationStatus_ = tr("Searching cities…");
  emit sitesChanged();
  auto *network = new QNetworkAccessManager(this);
  QUrl url("https://geocoding-api.open-meteo.com/v1/search");
  QUrlQuery query;
  query.addQueryItem("name", name.trimmed());
  query.addQueryItem("count", "6");
  query.addQueryItem("language",
                     QLocale().language() == QLocale::French ? "fr" : "en");
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setTransferTimeout(12000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = network->get(request);
  connect(reply, &QNetworkReply::readyRead, reply, [reply] {
    if (reply->bytesAvailable() > 65536)
      reply->abort();
  });
  connect(reply, &QNetworkReply::finished, this, [this, network, reply] {
    locating_ = false;
    locationStatus_ = tr("City search unavailable. Choose a preset or enter "
                         "coordinates in Advanced.");
    if (reply->error() == QNetworkReply::NoError &&
        reply->bytesAvailable() <= 65536) {
      QJsonParseError error;
      auto json = QJsonDocument::fromJson(reply->readAll(), &error);
      if (error.error == QJsonParseError::NoError &&
          !json.object().value("error").toBool()) {
        for (const auto &v : json.object().value("results").toArray()) {
          const auto r = v.toObject();
          const auto zone = r["timezone"].toString();
          const double lat = r["latitude"].toDouble(NAN),
                       lon = r["longitude"].toDouble(NAN),
                       height = r["elevation"].toDouble(0);
          if (!std::isfinite(lat) || !std::isfinite(lon) ||
              std::abs(lat) > 90 || std::abs(lon) > 180 ||
              !std::isfinite(height) || height < -500 || height > 10000 ||
              !QTimeZone(zone.toUtf8()).isValid())
            continue;
          auto s = site(r["name"].toString().left(100) + " · " +
                            r["admin1"].toString().left(100) + " · " +
                            r["country"].toString().left(100),
                        lat, lon, zone);
          s["elevation"] = height;
          s["source"] = "GeoNames · Open-Meteo";
          cityResults_.append(s);
        }
        locationStatus_ = cityResults_.isEmpty()
                              ? tr("No matching city. Try the city name "
                                   "followed by its country.")
                              : tr("Choose the matching city below.");
      }
    }
    reply->deleteLater();
    network->deleteLater();
    emit sitesChanged();
  });
}
bool ObservationController::selectCity(int index) {
  if (index < 0 || index >= cityResults_.size())
    return false;
  auto options = settings_;
  const auto s = cityResults_[index].toMap();
  for (auto it = s.begin(); it != s.end(); ++it)
    options[it.key()] = it.value();
  return calculate(options);
}
} // namespace astro::app
