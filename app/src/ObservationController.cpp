#include "ObservationController.hpp"
#include "TargetResolver.hpp"
#include "astronomy.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkInformation>
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
  catalogCount_ = int(catalog_.size());
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
        !std::isfinite(s.value("longitude").toDouble()) ||
        std::abs(s.value("longitude").toDouble()) > 180 ||
        !std::isfinite(s.value("elevation").toDouble()) ||
        s.value("elevation").toDouble() < -500 ||
        s.value("elevation").toDouble() > 10000 || sites_.size() >= 39)
      continue;
    s["favorite"] = true;
    sites_.append(s);
  }
  for (const auto &value :
       QSettings().value("observation/targetFavorites").toList()) {
    auto f = value.toMap();
    const QString id = f.value("id").toString();
    auto found = std::find_if(catalog_.begin(), catalog_.end(),
                              [&](const auto &t) { return t.id == id; });
    if (found == catalog_.end()) {
      bool raOk, decOk;
      const double ra = f.value("ra").toDouble(&raOk),
                   dec = f.value("dec").toDouble(&decOk);
      if ((!id.startsWith("cds:") && !id.startsWith("manual:")) || !raOk ||
          !decOk || !std::isfinite(ra) || !std::isfinite(dec) || ra < 0 ||
          ra >= 360 || std::abs(dec) > 90 ||
          f.value("name").toString().isEmpty())
        continue;
      catalog_.push_back({id, f.value("name").toString().left(512),
                          f.value("source").toString().left(512), -1, ra, dec});
    }
    if (favoriteTargets_.size() < 100 &&
        std::none_of(
            favoriteTargets_.begin(), favoriteTargets_.end(),
            [&](const auto &v) { return v.toMap().value("id") == id; }))
      favoriteTargets_.append(f);
  }
  QNetworkInformation::loadDefaultBackend();
  if (auto *network = QNetworkInformation::instance()) {
    updateReachability(int(network->reachability()));
    connect(network, &QNetworkInformation::reachabilityChanged, this,
            [this](auto state) { updateReachability(int(state)); });
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
  if (targetId == "manual" || targetId.startsWith("manual:") ||
      targetId.startsWith("cds:")) {
    target_ = {
        targetId,
        settings_.value("targetName", tr("Manual coordinates")).toString(),
        settings_.value("targetSource", tr("Manual J2000 coordinates"))
            .toString(),
        -1,
        settings_.value("ra").toDouble(),
        settings_.value("dec").toDouble()};
    if (targetId != "manual" &&
        std::none_of(catalog_.begin(), catalog_.end(),
                     [&](const auto &t) { return t.id == targetId; }))
      catalog_.push_back(target_);
  }
  for (int i = 0; i < sites_.size(); ++i) {
    auto s = sites_[i].toMap();
    s["label"] = (s.value("favorite").toBool() ? QString("★ ") : QString()) +
                 s.value("name").toString();
    sites_[i] = s;
  }
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
  if (offline_) {
    searchStatus_ = networkFailure(QNetworkReply::NetworkSessionFailedError,
                                   connectionLost_);
    emit searchChanged();
    return;
  }
  searching_ = true;
  searchStatus_ = tr("Searching CDS Sesame…");
  emit searchChanged();
  const int generation = ++searchGeneration_;
  resolver_ = std::jthread([this, query, generation](std::stop_token stop) {
    QVariantMap answer;
    QString error;
    int networkCode = 0;
    try {
      answer = resolveTargetPosition(query, stop);
    } catch (const TargetResolverNetworkError &e) {
      networkCode = e.code;
    } catch (const std::exception &e) {
      error = QString::fromUtf8(e.what());
    }
    if (stop.stop_requested())
      return;
    QMetaObject::invokeMethod(
        this,
        [this, answer, error, networkCode, generation] {
          searching_ = false;
          if (generation == searchGeneration_) {
            if (networkCode) {
              searchStatus_ = networkFailure(networkCode, connectionLost_);
            } else if (error.isEmpty()) {
              const QString id = "cds:" + answer.value("query").toString();
              catalog_.erase(
                  std::remove_if(catalog_.begin(), catalog_.end(),
                                 [&](const auto &t) { return t.id == id; }),
                  catalog_.end());
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
  s["target"] = target_.id;
  s["targetSource"] = target_.source;
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
    const auto solar = observationSolarEvents(s, start.toSecsSinceEpoch(),
                                              end.toSecsSinceEpoch());
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
    QVariantList solarList, nightList, daylightList;
    bool night = observationSunAltitude(s, start_) <= -18,
         daylight = observationSunIsUp(s, start_);
    qint64 nightBegin = start_, dayBegin = start_;
    QString sunrise, sunset;
    for (const auto &event : solar) {
      QString label;
      if (event.kind == "sunrise") {
        label = tr("Sunrise");
        if (sunrise.isEmpty())
          sunrise = localTime(event.utc);
        daylight = true;
        dayBegin = event.utc;
      }
      if (event.kind == "sunset") {
        label = tr("Sunset");
        if (sunset.isEmpty())
          sunset = localTime(event.utc);
        if (daylight)
          daylightList.append(QVariantMap{{"start", localTime(dayBegin)},
                                          {"end", localTime(event.utc)}});
        daylight = false;
      }
      if (event.kind == "civilDusk")
        label = tr("End of civil twilight · Sun −6°");
      if (event.kind == "civilDawn")
        label = tr("Start of civil twilight · Sun −6°");
      if (event.kind == "nightStart") {
        label = tr("Astronomical night begins · Sun −18°");
        night = true;
        nightBegin = event.utc;
      }
      if (event.kind == "nightEnd") {
        label = tr("Astronomical night ends · Sun −18°");
        if (night)
          nightList.append(QVariantMap{{"start", localTime(nightBegin)},
                                       {"end", localTime(event.utc)}});
        night = false;
      }
      solarList.append(QVariantMap{
          {"kind", event.kind},
          {"name", label},
          {"time", localTime(event.utc)},
          {"utc", event.utc},
          {"fraction", double(event.utc - start_) / (end_ - start_)}});
    }
    if (night)
      nightList.append(QVariantMap{{"start", localTime(nightBegin)},
                                   {"end", localTime(end_)}});
    if (daylight)
      daylightList.append(QVariantMap{{"start", localTime(dayBegin)},
                                      {"end", localTime(end_)}});
    result_["solarEvents"] = solarList;
    result_["sunrise"] =
        sunrise.isEmpty() ? tr("No sunrise in this period") : sunrise;
    result_["sunset"] =
        sunset.isEmpty() ? tr("No sunset in this period") : sunset;
    result_["nightSlots"] = nightList;
    result_["noAstronomicalNight"] = nightList.isEmpty();
    result_["daylightSlots"] = daylightList;
    result_["polarSun"] =
        sunrise.isEmpty() && sunset.isEmpty()
            ? (daylight
                   ? tr("Sun stays above the horizon throughout this period")
                   : tr("Sun stays below the horizon throughout this period"))
            : QString();
    samples_ = list;
    error_.clear();
    persist();
    updateInstant();
    updateRecommendations();
    emit favoritesChanged();
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
              {"solarPhase", observationSunIsUp(site_, utc) ? tr("Day")
                             : sun > -6  ? tr("Civil twilight")
                             : sun > -12 ? tr("Nautical twilight")
                             : sun > -18 ? tr("Astronomical twilight")
                                         : tr("Astronomical night")},
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
  if (name.trimmed().isEmpty() || name.size() > 100) {
    error_ = tr("Enter a favourite place name (maximum 100 characters).");
    emit changed();
    return false;
  }
  auto s = site(name.trimmed(), site_.latitude, site_.longitude,
                settings_["zone"].toString());
  s["elevation"] = site_.elevation;
  s["source"] = settings_.value("source");
  s["name"] = name.trimmed();
  s["favorite"] = true;
  s["label"] = "★ " + name.trimmed();
  int existing = -1;
  for (int i = 0; i < sites_.size(); ++i) {
    auto old = sites_[i].toMap();
    if (old.value("favorite").toBool() &&
        old.value("latitude") == s.value("latitude") &&
        old.value("longitude") == s.value("longitude") &&
        old.value("elevation") == s.value("elevation") &&
        old.value("zone") == s.value("zone")) {
      existing = i;
      break;
    }
  }
  if (existing < 0 && sites_.size() >= 39) {
    error_ = tr(
        "30 favourite places already saved. Remove one before adding another.");
    emit changed();
    return false;
  }
  error_.clear();
  if (existing >= 0)
    sites_[existing] = s;
  else
    sites_.append(s);
  settings_["name"] = name.trimmed();
  persist();
  emit changed();
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
  if (offline_) {
    locationStatus_ = networkFailure(QNetworkReply::NetworkSessionFailedError,
                                     connectionLost_);
    emit sitesChanged();
    return;
  }
  locating_ = true;
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
        QVariantList cities;
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
          cities.append(s);
        }
        cityResults_ = cities;
        locationStatus_ = cityResults_.isEmpty()
                              ? tr("No matching city. Try the city name "
                                   "followed by its country.")
                              : tr("Choose the matching city below.");
      }
    }
    if (reply->error() != QNetworkReply::NoError)
      locationStatus_ = networkFailure(int(reply->error()), connectionLost_);
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
QString ObservationController::networkFailure(int code, bool interrupted) {
  if (interrupted || code == QNetworkReply::RemoteHostClosedError ||
      code == QNetworkReply::TemporaryNetworkFailureError)
    return tr("Connection interrupted. Retry when Internet is available. "
              "Previous results and offline calculations are preserved.");
  if (code == QNetworkReply::NetworkSessionFailedError)
    return tr(
        "No Internet connection. Online search is unavailable. The catalogue, "
        "favourites and calculations remain available offline.");
  if (code == QNetworkReply::HostNotFoundError ||
      code == QNetworkReply::ConnectionRefusedError)
    return tr("Cannot reach the online service. Check your Internet connection "
              "and retry. Previous results are preserved.");
  if (code == QNetworkReply::TimeoutError ||
      code == QNetworkReply::OperationCanceledError)
    return tr("The online search timed out or was interrupted. Check your "
              "connection and retry. Previous results are preserved.");
  return tr("Online service unavailable. Retry later. Previous results and "
            "offline calculations are preserved.");
}
void ObservationController::updateReachability(int state) {
  using R = QNetworkInformation::Reachability;
  const bool offline = state == int(R::Disconnected) ||
                       state == int(R::Local) || state == int(R::Site);
  if (offline) {
    if (reachability_ == int(R::Online))
      connectionLost_ = true;
    offline_ = true;
    connectionStatus_ = networkFailure(QNetworkReply::NetworkSessionFailedError,
                                       connectionLost_);
  } else if (state == int(R::Online)) {
    connectionStatus_ =
        offline_ ? tr("Connection restored. You can retry the online search; "
                      "offline results are unchanged.")
                 : QString();
    offline_ = false;
    connectionLost_ = false;
  }
  reachability_ = state;
  emit connectionChanged();
}
bool ObservationController::targetFavorite() const {
  return std::any_of(favoriteTargets_.begin(), favoriteTargets_.end(),
                     [&](const auto &v) {
                       return v.toMap().value("id").toString() == target_.id;
                     });
}
void ObservationController::saveFavoriteTargets() {
  QSettings().setValue("observation/targetFavorites", favoriteTargets_);
  recommendationsKey_.clear();
  updateRecommendations();
  emit favoritesChanged();
  emit changed();
}
void ObservationController::toggleFavoriteTarget() {
  error_.clear();
  for (int i = 0; i < favoriteTargets_.size(); ++i) {
    if (favoriteTargets_[i].toMap().value("id").toString() == target_.id) {
      favoriteTargets_.removeAt(i);
      saveFavoriteTargets();
      return;
    }
  }
  if (favoriteTargets_.size() >= 100) {
    error_ = tr("100 favourite objects already saved. Remove one before adding "
                "another.");
    emit changed();
    return;
  }
  if (target_.id == "manual") {
    target_.id = "manual:" + QString::number(target_.ra, 'g', 17) + ":" +
                 QString::number(target_.dec, 'g', 17);
    if (std::none_of(catalog_.begin(), catalog_.end(),
                     [&](const auto &t) { return t.id == target_.id; }))
      catalog_.push_back(target_);
    settings_["target"] = target_.id;
    persist();
  }
  favoriteTargets_.append(QVariantMap{{"id", target_.id},
                                      {"name", target_.name},
                                      {"source", target_.source},
                                      {"body", target_.body},
                                      {"ra", target_.ra},
                                      {"dec", target_.dec}});
  saveFavoriteTargets();
}
void ObservationController::updateRecommendations() {
  const QString key =
      QString::number(start_) + ":" + QString::number(end_) + ":" +
      QString::number(site_.latitude, 'g', 17) + ":" +
      QString::number(site_.longitude, 'g', 17) + ":" +
      QString::number(site_.elevation, 'g', 17) + ":" +
      QString::number(settings_["minimum"].toDouble(), 'g', 17) + ":" +
      settings_["zone"].toString();
  if (recommendationsKey_ == key) {
    result_["recommendationPool"] = recommendationPool_;
    return;
  }
  const double minimum = settings_["minimum"].toDouble();
  const QStringList extra = {"NGC3372", "NGC5139", "NGC5128",
                             "NGC0104", "NGC2070", "NGC0253",
                             "NGC7000", "NGC0869", "NGC0884"};
  struct Candidate {
    ObservationTarget target;
    double score, limit;
  };
  std::vector<Candidate> candidates;
  std::vector<std::pair<qint64, double>> solar;
  for (qint64 utc = start_; utc <= end_; utc += 300)
    solar.push_back({utc, observationSunAltitude(site_, utc)});
  int pool = 0;
  for (const auto &t : catalog_) {
    const bool favorite = std::any_of(
        favoriteTargets_.begin(), favoriteTargets_.end(), [&](const auto &v) {
          return v.toMap().value("id").toString() == t.id;
        });
    if (t.body < 0 && !t.name.startsWith('M') && !extra.contains(t.id) &&
        !favorite)
      continue;
    // All built-in Messier designations start with M<number>; exclude unrelated
    // common names.
    if (t.body < 0 &&
        !QRegularExpression("^M[0-9]+").match(t.name).hasMatch() &&
        !extra.contains(t.id) && !favorite)
      continue;
    ++pool;
    const double limit = t.body >= 0 ? -6 : -18;
    double peak = -90;
    int count = 0;
    for (const auto &[utc, sun] : solar) {
      if (sun > limit)
        continue;
      const auto position = observationPosition(t, site_, utc);
      if (position.altitude < minimum)
        continue;
      // Keep solar neighbours out of beginner suggestions when they are within
      // 30° of the Sun.
      if ((t.body == BODY_MERCURY || t.body == BODY_VENUS) &&
          observationSeparation(
              position,
              observationPosition({{}, {}, {}, BODY_SUN}, site_, utc)) < 30)
        continue;
      ++count;
      peak = std::max(peak, position.altitude);
    }
    if (count >= 4)
      candidates.push_back(
          {t, count * 5 + peak * 3 + (favorite ? 10000 : 0), limit});
  }
  std::stable_sort(
      candidates.begin(), candidates.end(),
      [](const auto &a, const auto &b) { return a.score > b.score; });
  QVariantList suggestions;
  int movingCount = 0, fixedCount = 0;
  for (const auto &candidate : candidates) {
    const auto &t = candidate.target;
    if ((t.body >= 0 && movingCount >= 4) || (t.body < 0 && fixedCount >= 6))
      continue;
    const auto windows =
        observationWindows(t, site_, start_, end_, minimum, candidate.limit);
    qint64 minutes = 0, bestTime = start_;
    double peak = -90;
    QVariantList eligibleSlots;
    for (const auto &w : windows) {
      eligibleSlots.append(QVariantMap{{"start", localTime(w.start)},
                                       {"end", localTime(w.end)},
                                       {"startUtcSeconds", w.start},
                                       {"endUtcSeconds", w.end}});
      minutes += (w.end - w.start) / 60;
      for (qint64 utc = w.start; utc < w.end; utc += 60) {
        const double alt = observationPosition(t, site_, utc).altitude;
        if (alt > peak) {
          peak = alt;
          bestTime = utc;
        }
      }
    }
    if (minutes < 20)
      continue;
    // Apply the same solar separation to the full returned intervals, not just
    // ranking samples.
    if (t.body == BODY_MERCURY || t.body == BODY_VENUS) {
      bool safe = true;
      for (const auto &w : windows)
        for (qint64 utc = w.start; utc <= w.end; utc += 60)
          if (observationSeparation(
                  observationPosition(t, site_, utc),
                  observationPosition({{}, {}, {}, BODY_SUN}, site_, utc)) < 30)
            safe = false;
      if (!safe)
        continue;
    }
    suggestions.append(QVariantMap{{"id", t.id},
                                   {"name", t.name},
                                   {"category", t.body == BODY_MOON ? tr("Moon")
                                                : t.body >= 0 ? tr("Planet")
                                                              : tr("Deep sky")},
                                   {"sunLimit", candidate.limit},
                                   {"minimum", minimum},
                                   {"minutes", minutes},
                                   {"peakAltitude", peak},
                                   {"peakTime", localTime(bestTime)},
                                   {"slots", eligibleSlots},
                                   {"ra", t.ra},
                                   {"dec", t.dec},
                                   {"body", t.body}});
    if (t.body >= 0)
      ++movingCount;
    else
      ++fixedCount;
    if (movingCount + fixedCount == 10)
      break;
  }
  recommendations_ = suggestions;
  recommendationsKey_ = key;
  recommendationPool_ = pool;
  result_["recommendationPool"] = pool;
}
} // namespace astro::app
