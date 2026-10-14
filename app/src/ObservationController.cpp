#include "ObservationController.hpp"
#include "ObservationWeather.hpp"
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
#include <QSet>
#include <QSettings>
#include <QTimeZone>
#include <QUrlQuery>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
namespace astro::app {
namespace {
QString normalized(QString s) {
  static const QRegularExpression separators("[\\s\\p{M}_-]");
  s = s.toCaseFolded().normalized(QString::NormalizationForm_D);
  s.remove(separators);
  return s;
}
QString designation(QString s) {
  static const QRegularExpression pattern("^(NGC|IC|M|B|C)(0*)([0-9]+)(.*)$",
                                          QRegularExpression::CaseInsensitiveOption);
  auto m = pattern.match(s);
  return m.hasMatch() ? m.captured(1).toUpper() + m.captured(3) + m.captured(4) : s;
}
bool validCoordinates(double ra, double dec) {
  return std::isfinite(ra) && std::isfinite(dec) && ra >= 0 && ra < 360 &&
         std::abs(dec) <= 90;
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
QString validatedWebsite(const QString &text) {
  const auto trimmed = text.trimmed();
  const QUrl url(trimmed, QUrl::StrictMode);
  if (trimmed.size() > 2048 || !url.isValid() || url.host().isEmpty() ||
      (url.scheme() != "https" && url.scheme() != "http") ||
      !url.userInfo().isEmpty())
    return {};
  return url.toString(QUrl::FullyEncoded);
}
bool recommendationTarget(const ObservationTarget &target, bool favorite) {
  static const QStringList extra = {"NGC3372", "NGC5139", "NGC5128",
                                    "NGC0104", "NGC2070", "NGC0253",
                                    "NGC7000", "NGC0869", "NGC0884"};
  static const QRegularExpression messier("^M[0-9]+");
  return target.body >= 0 || favorite || extra.contains(target.id) ||
         (target.name.startsWith('M') && messier.match(target.name).hasMatch());
}
} // namespace
ObservationController::ObservationController(QObject *parent)
    : QObject(parent) {
  mapZoneDebounce_.setSingleShot(true);
  mapZoneDebounce_.setInterval(600);
  connect(&mapZoneDebounce_, &QTimer::timeout, this,
          &ObservationController::fetchMapTimeZone);
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
  // Published T68 location, checked 2026-10-12:
  // https://support.itelescope.net/support/solutions/articles/245471-telescope-t68
  // This is a geographic preset only; it has no provider integration.
  auto t68 = site(tr("T68 · Utah"), 37.7378, -113.6975, "America/Denver");
  t68["elevation"] = 1570;
  sites_.append(t68);
  const QStringList presetIds = {"paris", "lyon", "marseille", "la-serena",
                                "sydney", "tokyo", "new-york", "tromso",
                                "cape-town", "t68-utah"};
  const auto presetPreferences =
      QSettings().value("observation/presetSites").toMap();
  for (int i = 0; i < sites_.size(); ++i) {
    auto s = sites_[i].toMap();
    s["presetId"] = presetIds[i];
    s["label"] = s.value("name");
    sites_[i] = s;
  }
  presetSites_ = sites_;
  for (int i = sites_.size() - 1; i >= 0; --i) {
    const auto preference = presetPreferences.value(presetIds[i]).toMap();
    if (preference.value("hidden").toBool()) {
      sites_.removeAt(i);
      continue;
    }
    auto s = sites_[i].toMap();
    s["presetId"] = presetIds[i];
    s["favorite"] = preference.value("favorite", false);
    sites_[i] = s;
  }
  // Existing versions stored saved places as favourites. Preserve all of them
  // while allowing a saved place to remain in the menu without its star.
  const bool legacySites = !QSettings().contains("observation/savedSites");
  const auto stored = QSettings()
                          .value(legacySites ? "observation/favorites"
                                             : "observation/savedSites")
                          .toList();
  int savedCount = 0;
  for (const auto &value : stored) {
    auto s = value.toMap();
    bool latitudeOk, longitudeOk, elevationOk;
    s.value("latitude").toDouble(&latitudeOk);
    s.value("longitude").toDouble(&longitudeOk);
    s.value("elevation").toDouble(&elevationOk);
    if (!latitudeOk || !longitudeOk || !elevationOk ||
        s.value("name").toString().trimmed().isEmpty() ||
        !QTimeZone(s.value("zone").toString().toUtf8()).isValid() ||
        !std::isfinite(s.value("latitude").toDouble()) ||
        std::abs(s.value("latitude").toDouble()) > 90 ||
        !std::isfinite(s.value("longitude").toDouble()) ||
        std::abs(s.value("longitude").toDouble()) > 180 ||
        !std::isfinite(s.value("elevation").toDouble()) ||
        s.value("elevation").toDouble() < -500 ||
        s.value("elevation").toDouble() > 10000 ||
        savedCount >= 30)
      continue;
    s.remove("presetId");
    s["saved"] = true;
    s["favorite"] = legacySites || s.value("favorite").toBool();
    s["website"] = validatedWebsite(s.value("website").toString());
    sites_.append(s);
    ++savedCount;
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
    settings_ = site(tr("Paris"), 48.8566, 2.3522, "Europe/Paris");
  auto initialPlanning = settings_;
  initialPlanning.remove("date");
  const auto planning = planningNight(initialPlanning);
  settings_["date"] =
      planning.value("currentDate", QDate::currentDate().toString(Qt::ISODate));
  settings_["minimum"] = settings_.value("minimum", 30);
  settings_["sunLimit"] = settings_.value("sunLimit", -18);
  if (settings_.value("referenceZone").toString() != "site")
    settings_["referenceZone"] = "system";
  target_ = catalog_[0];
  const QString targetId = settings_.value("target", "NGC1976").toString();
  for (const auto &t : catalog_)
    if (t.id == targetId) {
      target_ = t;
      break;
    }
  if (targetId == "manual" || targetId.startsWith("manual:") ||
      targetId.startsWith("cds:")) {
    bool raOk, decOk;
    const double ra = settings_.value("ra").toDouble(&raOk),
                 dec = settings_.value("dec").toDouble(&decOk);
    target_ = {
        targetId,
        settings_.value("targetName", tr("Manual coordinates")).toString(),
        settings_.value("targetSource", tr("Manual J2000 coordinates"))
            .toString(),
        -1,
        raOk ? ra : NAN,
        decOk ? dec : NAN};
    if (targetId != "manual" && validCoordinates(target_.ra, target_.dec) &&
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
  weatherAgeTimer_.setInterval(60000);
  connect(&weatherAgeTimer_, &QTimer::timeout, this, [this] {
    if (!weatherForecast_.isEmpty())
      emit weatherChanged();
    updateReference();
    emit referenceChanged();
  });
  weatherAgeTimer_.start();
  weatherDebounce_.setSingleShot(true);
  weatherDebounce_.setInterval(600);
  connect(&weatherDebounce_, &QTimer::timeout, this,
          [this] { refreshWeather(false); });
  const auto cachedWeather =
      QSettings().value("observation/weatherCache").toMap();
  const auto cachedAt = cachedWeather.value("receivedUtc").toLongLong();
  if (cachedAt > 0 && cachedAt <= QDateTime::currentSecsSinceEpoch() + 60 &&
      QDateTime::currentSecsSinceEpoch() - cachedAt < 7 * 86400) {
    try {
      weatherForecast_ = parseObservationWeather(
          cachedWeather.value("data").toByteArray(),
          cachedWeather.value("latitude", NAN).toDouble(),
          cachedWeather.value("longitude", NAN).toDouble(), cachedAt);
    } catch (const std::exception &) {
    }
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
  if (query.size() > 100) {
    searchStatus_ = tr("Use a name or catalogue number of up to 100 characters.");
    emit searchChanged();
    return;
  }
  const auto q = normalized(designation(normalized(query)));
  if (q == "ngc" || q == "ic" || q == "m" || q == "b" || q == "c") {
    searchStatus_ = tr("Add a catalogue number, for example NGC 5078 or M42.");
    emit searchChanged();
    return;
  }
  if (!q.isEmpty() && q.size() < 2) {
    searchStatus_ = tr("Enter at least two characters or a catalogue number.");
    emit searchChanged();
    return;
  }
  constexpr int maximumResults = 25;
  std::array<QVariantList, 3> ranked;
  int matchCount = 0;
  const QStringList suggested = {"moon", "jupiter", "saturn",
                                 "mars", "NGC1976", "NGC0224",
                                 "M045", "NGC6205", "NGC3372"};
  for (const auto &t : catalog_) {
    auto keys = searchKeys_.constFind(t.id);
    if (keys == searchKeys_.cend()) {
      searchKeys_.insert(t.id, {normalized(t.name),
                               normalized(designation(t.id)), normalized(t.id),
                               normalized(t.name.section(" · ", 0, 0))});
      keys = searchKeys_.constFind(t.id);
    }
    if (!(q.isEmpty() ? suggested.contains(t.id)
                     : keys->name.contains(q) || keys->designation == q ||
                           keys->id == q))
      continue;
    ++matchCount;
    const int rank = !q.isEmpty() &&
                             (keys->designation == q || keys->id == q ||
                              keys->firstName == q)
                         ? 0
                         : t.body >= 0 ? 1 : 2;
    if (ranked[rank].size() < maximumResults)
      ranked[rank].append(QVariantMap{{"id", t.id}, {"name", t.name},
                                      {"source", t.source},
                                      {"moving", t.body >= 0}});
  }
  for (const auto &matches : ranked) {
    for (const auto &value : matches) {
      if (searchResults_.size() == maximumResults)
        break;
      searchResults_.append(value);
    }
  }
  if (matchCount > maximumResults)
    searchStatus_ = tr("25 results shown. Refine the name or catalogue number.");
  emit searchChanged();
}
void ObservationController::searchOnline(const QString &query) {
  const auto q = normalized(query.left(101));
  if (query.size() > 100 || q.size() < 2 || q == "ngc" || q == "ic") {
    search(query);
    return;
  }
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
              searchKeys_.remove(id);
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
  // Keep the recoverable input intact until a valid target is selected.
  if (target_.body < 0 && !validCoordinates(target_.ra, target_.dec))
    return;
  QSettings prefs;
  auto s = settings_;
  s["target"] = target_.id;
  s["targetSource"] = target_.source;
  s["targetName"] = target_.name;
  s["ra"] = target_.ra;
  s["dec"] = target_.dec;
  prefs.setValue("observation/settings", s);
}
QVariantMap
ObservationController::planningNight(const QVariantMap &options) const {
  try {
    auto number = [&](const char *key) {
      bool ok = false;
      const double value = QString(options.value(key).toString())
                               .replace(',', '.')
                               .toDouble(&ok);
      if (!ok || !std::isfinite(value))
        throw std::runtime_error("Enter valid numerical values.");
      return value;
    };
    const ObservationSite site{number("latitude"), number("longitude"),
                               number("elevation")};
    const QString zone = options.value("zone").toString();
    const auto now = QDateTime::currentSecsSinceEpoch();
    const auto currentDate = observationPlanningDate(site, zone, now);
    const auto date = options.value("date", currentDate).toString();
    const auto end = observationNightEnd(site, date, zone);
    return {
        {"past", end <= now}, {"currentDate", currentDate}, {"endUtc", end}};
  } catch (const std::exception &e) {
    return {{"error", tr(e.what())}};
  }
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
    const auto referenceSource =
        options.value("referenceZone", "system").toString();
    if (referenceSource != "system" && referenceSource != "site")
      throw std::runtime_error(
          "Choose this computer or the telescope for the reference time zone.");
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
    auto siteSolar = solar;
    const auto followingEnd = end.toSecsSinceEpoch() + 86400;
    if (QDateTime::fromSecsSinceEpoch(followingEnd, QTimeZone::UTC)
            .date()
            .year() <= 2100) {
      const auto following =
          observationSolarEvents(s, end.toSecsSinceEpoch(), followingEnd);
      siteSolar.insert(siteSolar.end(), following.begin(), following.end());
    }
    target_ = t;
    site_ = s;
    settings_ = options;
    settings_["latitude"] = s.latitude;
    settings_["longitude"] = s.longitude;
    settings_["elevation"] = s.elevation;
    settings_["referenceZone"] = referenceSource;
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
          {"startUtc", w.start},
          {"endUtc", w.end},
          {"startFraction", double(w.start - start_) / (end_ - start_)},
          {"endFraction", double(w.end - start_) / (end_ - start_)},
          {"minutes", (w.end - w.start) / 60}});
    result_ = {
        {"startUtcSeconds", start_},
        {"endUtcSeconds", end_},
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
    QVariantList siteEvents;
    for (const auto &event : siteSolar)
      siteEvents.append(QVariantMap{{"kind", event.kind},
                                    {"utc", event.utc},
                                    {"time", localTime(event.utc)}});
    result_["siteSolarEvents"] = siteEvents;
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
    updateWeatherContext();
    confirmMapSites();
    emit favoritesChanged();
    emit changed();
    emit weatherChanged();
    return true;
  } catch (const std::exception &e) {
    error_ = tr(e.what());
    emit changed();
    return false;
  }
}
bool ObservationController::changeSite(const QVariantMap &options) {
  if (!start_)
    return calculate(options);
  return calculateAtUtc(options, instant_.value("utcSeconds").toLongLong());
}
bool ObservationController::showNow() {
  const auto utc = QDateTime::currentSecsSinceEpoch();
  if (utc >= start_ && utc < end_)
    return setReferenceUtc(utc);
  return calculateAtUtc(settings_, utc);
}
bool ObservationController::calculateAtUtc(const QVariantMap &options,
                                           qint64 utc) {
  const double previousFraction = fraction_;
  try {
    const QTimeZone zone(options.value("zone").toString().toUtf8());
    if (!zone.isValid())
      throw std::runtime_error("Enter a valid date (1900–2100) and time zone.");
    const auto local = QDateTime::fromSecsSinceEpoch(utc, zone);
    const auto date =
        local.time() < QTime(12, 0) ? local.date().addDays(-1) : local.date();
    auto anchored = options;
    anchored["date"] = date.toString(Qt::ISODate);
    const auto begin = observationNoon(anchored["date"].toString(),
                                       anchored["zone"].toString());
    const auto end = QDateTime(begin.date().addDays(1), QTime(12, 0), zone);
    // Set the new fraction before calculate publishes any instant: every view
    // and signal observes the same UTC time throughout a location change.
    fraction_ = double(utc - begin.toSecsSinceEpoch()) / begin.secsTo(end);
    if (calculate(anchored))
      return true;
  } catch (const std::exception &e) {
    error_ = tr(e.what());
    emit changed();
  }
  fraction_ = previousFraction;
  return false;
}
void ObservationController::setFraction(double value) {
  if (!std::isfinite(value) || !start_)
    return;
  fraction_ = std::clamp(value, 0.0, 1.0);
  updateInstant();
}
bool ObservationController::setReferenceZone(const QString &source) {
  if (source != "system" && source != "site")
    return false;
  settings_["referenceZone"] = source;
  persist();
  updateReference();
  emit changed();
  emit referenceChanged();
  return true;
}
QVariantMap
ObservationController::referenceTimeChoices(const QString &dateText,
                                            const QString &clockText) const {
  const auto date = QDate::fromString(dateText, Qt::ISODate);
  auto time = QTime::fromString(clockText.trimmed(), "HH:mm");
  if (!time.isValid())
    time = QTime::fromString(clockText.trimmed(), "H:mm");
  if (!date.isValid() || !time.isValid())
    return {{"error", tr("Enter a valid reference date and time (HH:mm).")}};
  const QTimeZone zone(reference_.value("zone").toString().toUtf8());
  const auto wallClock =
      QDateTime(date, time, QTimeZone::UTC).toSecsSinceEpoch();
  QSet<int> offsets;
  const auto start = QDateTime::fromSecsSinceEpoch(start_, QTimeZone::UTC);
  const auto end = QDateTime::fromSecsSinceEpoch(end_, QTimeZone::UTC);
  offsets.insert(zone.offsetFromUtc(start));
  offsets.insert(zone.offsetFromUtc(end));
  for (const auto &transition : zone.transitions(start, end))
    offsets.insert(transition.offsetFromUtc);
  std::vector<qint64> candidates;
  for (const auto offset : offsets) {
    const auto utc = wallClock - offset;
    const auto local = QDateTime::fromSecsSinceEpoch(utc, zone);
    if (utc >= start_ && utc <= end_ && local.date() == date &&
        local.time() == time)
      candidates.push_back(utc);
  }
  std::sort(candidates.begin(), candidates.end());
  QVariantList choices;
  for (const auto utc : candidates)
    choices.append(
        QVariantMap{{"utc", utc},
                    {"label", QDateTime::fromSecsSinceEpoch(utc, zone).toString(
                                  "dd/MM/yyyy HH:mm t")}});
  if (choices.isEmpty())
    return {{"error", tr("This time is outside the chosen night, or does not "
                         "exist because of a clock change.")}};
  return {{"choices", choices}};
}
bool ObservationController::setReferenceUtc(qint64 utc) {
  if (!start_ || utc < start_ || utc > end_)
    return false;
  setFraction(double(utc - start_) / (end_ - start_));
  return true;
}
QVariantMap ObservationController::recordingWindow(qint64 notBefore) const {
  if (!notBefore)
    notBefore = QDateTime::currentSecsSinceEpoch();
  const QTimeZone zone(reference_.value("zone").toString().toUtf8());
  for (const auto &value : result_.value("slots").toList()) {
    const auto slot = value.toMap();
    const auto begin = std::max(notBefore, slot.value("startUtc").toLongLong());
    // Recording times are whole minutes inside the geometric interval: never
    // present a rounded start before the object's height/darkness limits hold.
    const auto start = qint64(std::ceil(begin / 60.0)) * 60;
    const auto end =
        qint64(std::floor(slot.value("endUtc").toLongLong() / 60.0)) * 60;
    if (end <= start)
      continue;
    return {{"available", true},
            {"startUtc", start},
            {"endUtc", end},
            {"start", QDateTime::fromSecsSinceEpoch(start, zone)
                          .toString("dd/MM/yyyy HH:mm t")},
            {"end", QDateTime::fromSecsSinceEpoch(end, zone).toString(
                        "dd/MM/yyyy HH:mm t")},
            {"siteStart", localTime(start)},
            {"siteEnd", localTime(end)},
            {"homeStart",
             QDateTime::fromSecsSinceEpoch(start, QTimeZone::systemTimeZone())
                 .toString("dd/MM/yyyy HH:mm t")},
            {"homeEnd",
             QDateTime::fromSecsSinceEpoch(end, QTimeZone::systemTimeZone())
                 .toString("dd/MM/yyyy HH:mm t")},
            {"homeStartLocal",
             QDateTime::fromSecsSinceEpoch(start, QTimeZone::systemTimeZone())
                 .toString("dd/MM/yyyy HH:mm")},
            {"homeEndLocal",
             QDateTime::fromSecsSinceEpoch(end, QTimeZone::systemTimeZone())
                 .toString("dd/MM/yyyy HH:mm")},
            {"siteStartLocal",
             QDateTime::fromSecsSinceEpoch(
                 start, QTimeZone(settings_.value("zone").toString().toUtf8()))
                 .toString("dd/MM/yyyy HH:mm")},
            {"siteEndLocal",
             QDateTime::fromSecsSinceEpoch(
                 end, QTimeZone(settings_.value("zone").toString().toUtf8()))
                 .toString("dd/MM/yyyy HH:mm")},
            {"minutes", (end - start) / 60}};
  }
  return {{"available", false}};
}
void ObservationController::updateReference() {
  if (!start_ || !instant_.contains("utcSeconds"))
    return;
  const auto source = settings_.value("referenceZone", "system").toString();
  const QTimeZone siteZone(settings_.value("zone").toString().toUtf8());
  const auto homeZone = QTimeZone::systemTimeZone();
  if (!siteZone.isValid() || !homeZone.isValid())
    return;
  const auto zone = source == "site" ? siteZone : homeZone;
  const auto utc = instant_.value("utcSeconds").toLongLong();
  const auto local = QDateTime::fromSecsSinceEpoch(utc, zone);
  const auto first = QDateTime::fromSecsSinceEpoch(start_, zone).date();
  const auto last = QDateTime::fromSecsSinceEpoch(end_, zone).date();
  QVariantList days, referenceSlots;
  for (auto date = first; date <= last; date = date.addDays(1))
    days.append(QVariantMap{{"date", date.toString(Qt::ISODate)},
                            {"label", date.toString("dd/MM/yyyy")}});
  for (const auto &value : result_.value("slots").toList()) {
    auto slot = value.toMap();
    slot["referenceStart"] =
        QDateTime::fromSecsSinceEpoch(slot.value("startUtc").toLongLong(), zone)
            .toString("dd/MM/yyyy HH:mm:ss t");
    slot["referenceEnd"] =
        QDateTime::fromSecsSinceEpoch(slot.value("endUtc").toLongLong(), zone)
            .toString("dd/MM/yyyy HH:mm:ss t");
    const auto start = slot.value("startUtc").toLongLong(),
               end = slot.value("endUtc").toLongLong();
    slot["homeStart"] = QDateTime::fromSecsSinceEpoch(start, homeZone)
                            .toString("dd/MM/yyyy HH:mm:ss t");
    slot["homeEnd"] = QDateTime::fromSecsSinceEpoch(end, homeZone)
                          .toString("dd/MM/yyyy HH:mm:ss t");
    slot["siteStart"] = QDateTime::fromSecsSinceEpoch(start, siteZone)
                            .toString("dd/MM/yyyy HH:mm:ss t");
    slot["siteEnd"] = QDateTime::fromSecsSinceEpoch(end, siteZone)
                          .toString("dd/MM/yyyy HH:mm:ss t");
    slot["homeStartLocal"] = QDateTime::fromSecsSinceEpoch(start, homeZone)
                                 .toString("dd/MM/yyyy HH:mm:ss");
    slot["homeEndLocal"] = QDateTime::fromSecsSinceEpoch(end, homeZone)
                               .toString("dd/MM/yyyy HH:mm:ss");
    slot["siteStartLocal"] = QDateTime::fromSecsSinceEpoch(start, siteZone)
                                 .toString("dd/MM/yyyy HH:mm:ss");
    slot["siteEndLocal"] = QDateTime::fromSecsSinceEpoch(end, siteZone)
                               .toString("dd/MM/yyyy HH:mm:ss");
    referenceSlots.append(slot);
  }
  reference_ = {
      {"source", source},
      {"zone", QString::fromUtf8(zone.id())},
      {"homeZone", QString::fromUtf8(homeZone.id())},
      {"siteZone", QString::fromUtf8(siteZone.id())},
      {"differentZones", homeZone.id() != siteZone.id()},
      {"date", local.date().toString(Qt::ISODate)},
      {"clock", local.time().toString("HH:mm")},
      {"display", local.toString("dd/MM/yyyy HH:mm t")},
      {"homeDisplay", QDateTime::fromSecsSinceEpoch(utc, homeZone)
                          .toString("dd/MM/yyyy HH:mm t")},
      {"siteDisplay", QDateTime::fromSecsSinceEpoch(utc, siteZone)
                          .toString("dd/MM/yyyy HH:mm t")},
      {"homeLocalDisplay", QDateTime::fromSecsSinceEpoch(utc, homeZone)
                               .toString("dd/MM/yyyy HH:mm")},
      {"siteLocalDisplay", QDateTime::fromSecsSinceEpoch(utc, siteZone)
                               .toString("dd/MM/yyyy HH:mm")},
      {"days", days},
      {"slots", referenceSlots}};
  reference_["recording"] = recordingWindow();
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
      const int solarState = observationSunIsUp(s, utc) ? 0
                             : solar > -18              ? 1
                                                        : 2;
      world_.append(QVariantMap{{"lat", lat},
                                {"lon", lon},
                                {"state", state},
                                {"solarState", solarState}});
    }
  updateReference();
  emit instantChanged();
  emit referenceChanged();
  emit weatherChanged();
}
void ObservationController::chooseMapLocation(double lat, double lon) {
  auto s = settings_;
  s["name"] = tr("Map position");
  s["latitude"] = lat;
  s["longitude"] = lon;
  s["elevation"] = 0;
  s["zone"] = "UTC";
  s["mapZoneUnresolved"] = true;
  mapZoneStatus_ = tr("Time zone not determined · UTC until confirmed.");
  changeSite(s);
}
void ObservationController::resolveMapTimeZone() {
  if (!settings_.value("mapZoneUnresolved").toBool())
    return;
  ++mapZoneGeneration_;
  if (mapZoneReply_)
    mapZoneReply_->abort();
  mapZoneBusy_ = true;
  mapZoneStatus_ = tr("Finding this point's local time zone…");
  mapZoneDebounce_.start();
  emit changed();
}
void ObservationController::fetchMapTimeZone() {
  if (!settings_.value("mapZoneUnresolved").toBool()) {
    mapZoneBusy_ = false;
    return;
  }
  if (offline_) {
    mapZoneBusy_ = false;
    mapZoneStatus_ =
        networkFailure(QNetworkReply::NetworkSessionFailedError,
                       connectionLost_) +
        " " + tr("Set the time zone in Coordinates; UTC is shown meanwhile.");
    emit changed();
    return;
  }
  if (!weatherNetwork_)
    weatherNetwork_ = new QNetworkAccessManager(this);
  const auto latitude = site_.latitude, longitude = site_.longitude;
  QUrl url("https://api.open-meteo.com/v1/forecast");
  QUrlQuery query;
  query.addQueryItem("latitude", QString::number(latitude, 'g', 17));
  query.addQueryItem("longitude", QString::number(longitude, 'g', 17));
  query.addQueryItem("timezone", "auto");
  query.addQueryItem("forecast_days", "1");
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setTransferTimeout(12000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = weatherNetwork_->get(request);
  mapZoneReply_ = reply;
  const auto generation = mapZoneGeneration_;
  connect(reply, &QNetworkReply::readyRead, reply, [reply] {
    if (reply->bytesAvailable() > 8192)
      reply->abort();
  });
  QTimer::singleShot(15000, reply, [reply] {
    if (!reply->isFinished())
      reply->abort();
  });
  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, generation, latitude, longitude] {
        if (generation != mapZoneGeneration_) {
          reply->deleteLater();
          return;
        }
        mapZoneBusy_ = false;
        mapZoneReply_ = nullptr;
        if (site_.latitude != latitude || site_.longitude != longitude ||
            !settings_.value("mapZoneUnresolved").toBool()) {
          reply->deleteLater();
          return;
        }
        const auto zone = QJsonDocument::fromJson(reply->readAll())
                              .object()
                              .value("timezone")
                              .toString();
        if (reply->error() == QNetworkReply::NoError &&
            QTimeZone(zone.toUtf8()).isValid()) {
          auto options = settings_;
          options["zone"] = zone;
          options["mapZoneUnresolved"] = false;
          if (changeSite(options))
            mapZoneStatus_.clear();
        } else {
          mapZoneStatus_ =
              (reply->error() == QNetworkReply::NoError
                   ? tr("The local time zone could not be determined.")
                   : networkFailure(int(reply->error()), connectionLost_)) +
              " " +
              tr("Set the time zone in Coordinates; UTC is shown meanwhile.");
          emit changed();
        }
        reply->deleteLater();
      });
}
void ObservationController::confirmMapSites() {
  if (settings_.value("mapZoneUnresolved").toBool())
    return;
  bool updated = false;
  for (auto &value : sites_) {
    auto saved = value.toMap();
    if (!saved.value("saved").toBool() ||
        !saved.value("mapZoneUnresolved").toBool() ||
        saved.value("latitude").toDouble() != site_.latitude ||
        saved.value("longitude").toDouble() != site_.longitude)
      continue;
    saved["zone"] = settings_.value("zone");
    saved["mapZoneUnresolved"] = false;
    value = saved;
    updated = true;
  }
  if (updated) {
    persistSites();
    emit sitesChanged();
  }
}
void ObservationController::persistSites() {
  QVariantList saved, favorites;
  auto presets = QSettings().value("observation/presetSites").toMap();
  for (const auto &value : sites_) {
    const auto s = value.toMap();
    if (s.value("saved").toBool()) {
      saved.append(value);
      if (s.value("favorite").toBool())
        favorites.append(value);
    } else if (!s.value("presetId").toString().isEmpty()) {
      presets[s.value("presetId").toString()] =
          QVariantMap{{"favorite", s.value("favorite")}, {"hidden", false}};
    }
  }
  QSettings prefs;
  prefs.setValue("observation/savedSites", saved);
  prefs.setValue("observation/presetSites", presets);
  // Keep the original key readable by earlier application versions.
  prefs.setValue("observation/favorites", favorites);
}
bool ObservationController::saveSite(const QString &name, const QString &website) {
  if (name.trimmed().isEmpty() || name.size() > 100) {
    error_ = tr("Enter a place name (maximum 100 characters).");
    emit changed();
    return false;
  }
  const auto link = validatedWebsite(website);
  if (!website.trimmed().isEmpty() && link.isEmpty()) {
    error_ = tr("Enter an http:// or https:// address without login details (maximum 2048 characters), or leave it empty.");
    emit changed();
    return false;
  }
  auto s = site(name.trimmed(), site_.latitude, site_.longitude,
                settings_["zone"].toString());
  s["elevation"] = site_.elevation;
  s["mapZoneUnresolved"] = settings_.value("mapZoneUnresolved", false);
  s["source"] = settings_.value("source");
  s["name"] = name.trimmed();
  s["saved"] = true;
  s["favorite"] = true;
  s["label"] = "★ " + name.trimmed();
  int existing = -1;
  for (int i = 0; i < sites_.size(); ++i) {
    auto old = sites_[i].toMap();
    if (old.value("saved").toBool() &&
        old.value("latitude") == s.value("latitude") &&
        old.value("longitude") == s.value("longitude") &&
        old.value("elevation") == s.value("elevation") &&
        old.value("zone") == s.value("zone")) {
      existing = i;
      s["website"] = old.value("website");
      s["favorite"] = old.value("favorite");
      s["label"] = (s.value("favorite").toBool() ? QString("★ ") : QString()) +
                   name.trimmed();
      break;
    }
  }
  const int savedCount = std::count_if(sites_.begin(), sites_.end(),
                                     [](const auto &value) {
    return value.toMap().value("saved").toBool();
  });
  if (existing < 0 && savedCount >= 30) {
    error_ = tr(
        "30 places already saved. Remove one before adding another.");
    emit changed();
    return false;
  }
  if (!website.isNull())
    s["website"] = link;
  error_.clear();
  if (existing >= 0)
    sites_[existing] = s;
  else
    sites_.append(s);
  settings_["name"] = name.trimmed();
  persist();
  emit changed();
  persistSites();
  emit sitesChanged();
  return true;
}
bool ObservationController::hasHiddenPresetSites() const {
  for (const auto &preset : presetSites_)
    if (std::none_of(sites_.cbegin(), sites_.cend(), [&](const auto &value) {
          return value.toMap().value("presetId") ==
                 preset.toMap().value("presetId");
        }))
      return true;
  return false;
}
void ObservationController::restorePresetSites() {
  if (!hasHiddenPresetSites())
    return;
  for (const auto &preset : presetSites_)
    if (std::none_of(sites_.cbegin(), sites_.cend(), [&](const auto &value) {
          return value.toMap().value("presetId") ==
                 preset.toMap().value("presetId");
        }))
      sites_.append(preset);
  persistSites();
  emit sitesChanged();
}
void ObservationController::toggleFavoriteSite(int index) {
  if (index < 0 || index >= sites_.size())
    return;
  auto s = sites_[index].toMap();
  s["favorite"] = !s.value("favorite").toBool();
  s["label"] = (s.value("favorite").toBool() ? QString("★ ") : QString()) +
               s.value("name").toString();
  sites_[index] = s;
  persistSites();
  emit sitesChanged();
}
void ObservationController::removeSite(int index) {
  if (index < 0 || index >= sites_.size())
    return;
  const auto id = sites_[index].toMap().value("presetId").toString();
  if (!id.isEmpty()) {
    auto presets = QSettings().value("observation/presetSites").toMap();
    presets[id] = QVariantMap{{"hidden", true}};
    QSettings().setValue("observation/presetSites", presets);
  }
  sites_.removeAt(index);
  persistSites();
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
  return changeSite(options);
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
  emit weatherChanged();
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
  emit instantChanged();
  emit changed();
  emit weatherChanged();
}
void ObservationController::toggleFavoriteTarget() {
  if (target_.body < 0 && !validCoordinates(target_.ra, target_.dec)) {
    error_ = tr("Invalid J2000 coordinates.");
    emit changed();
    return;
  }
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
    if (!recommendationTarget(t, favorite))
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
QVariantList ObservationController::visibleTargets() const {
  if (!start_)
    return {};
  const auto utc = instant_.value("utcSeconds").toLongLong();
  const auto sun = instant_.value("sun").toDouble();
  const auto minimum = settings_.value("minimum").toDouble();
  QSet<QString> favorites;
  for (const auto &value : favoriteTargets_)
    favorites.insert(value.toMap().value("id").toString());
  struct Candidate {
    QVariantMap data;
    double score;
  };
  std::vector<Candidate> candidates;
  for (const auto &target : catalog_) {
    const bool favorite = favorites.contains(target.id);
    if (!recommendationTarget(target, favorite))
      continue;
    const double limit = target.body >= 0 ? -6 : -18;
    if (sun > limit)
      continue;
    const auto position = observationPosition(target, site_, utc);
    if (position.altitude < minimum)
      continue;
    if ((target.body == BODY_MERCURY || target.body == BODY_VENUS) &&
        observationSeparation(
            position, observationPosition({{}, {}, {}, BODY_SUN}, site_, utc)) <
            30)
      continue;
    candidates.push_back({{{"id", target.id},
                           {"name", target.name},
                           {"body", target.body},
                           {"category", target.body == BODY_MOON ? tr("Moon")
                                        : target.body >= 0 ? tr("Planet")
                                                           : tr("Deep sky")},
                           {"altitude", position.altitude},
                           {"azimuth", position.azimuth},
                           {"sunLimit", limit},
                           {"minimum", minimum},
                           {"ra", target.ra},
                           {"dec", target.dec}},
                          position.altitude + (favorite ? 10000 : 0)});
  }
  std::stable_sort(
      candidates.begin(), candidates.end(),
      [](const auto &a, const auto &b) { return a.score > b.score; });
  QVariantList targets;
  int moving = 0, fixed = 0;
  for (const auto &candidate : candidates) {
    const bool body = candidate.data.value("body").toInt() >= 0;
    if (body ? moving >= 4 : fixed >= 6)
      continue;
    targets.append(candidate.data);
    body ? ++moving : ++fixed;
  }
  return targets;
}
bool ObservationController::weatherPeriodSupported() const {
  const auto day = QDateTime::currentDateTimeUtc().date();
  const auto first = QDateTime(day.addDays(-1), QTime(0, 0), QTimeZone::UTC)
                         .toSecsSinceEpoch();
  const auto last = QDateTime(day.addDays(16), QTime(0, 0), QTimeZone::UTC)
                        .toSecsSinceEpoch();
  return start_ >= first && end_ < last;
}
QVariantList ObservationController::weatherSamples() const {
  QVariantList samples;
  if (!observationWeatherMatches(weatherForecast_, site_.latitude,
                                 site_.longitude) ||
      !start_)
    return samples;
  for (const auto &value : weatherForecast_.value("samples").toList()) {
    auto sample = value.toMap();
    const auto utc = sample.value("utc").toLongLong();
    if (utc < start_ || utc > end_)
      continue;
    sample["fraction"] = double(utc - start_) / (end_ - start_);
    sample["time"] = localTime(utc);
    samples.append(sample);
  }
  return samples;
}
QVariantMap ObservationController::weather() const {
  const bool matches = observationWeatherMatches(
      weatherForecast_, site_.latitude, site_.longitude);
  auto sample =
      matches ? observationWeatherAt(weatherForecast_,
                                     instant_.value("utcSeconds").toLongLong())
              : QVariantMap();
  const bool stale =
      matches && QDateTime::currentSecsSinceEpoch() -
                         weatherForecast_.value("receivedUtc").toLongLong() >
                     3600;
  sample["available"] = sample.value("available", false);
  sample["hasForecast"] = !weatherSamples().isEmpty();
  sample["busy"] = weatherBusy_;
  sample["failed"] = weatherFailed_;
  sample["supported"] = weatherPeriodSupported();
  sample["canRefresh"] = !weatherBusy_ && !offline_ && weatherPeriodSupported();
  sample["stale"] = stale;
  sample["status"] =
      weatherStatus_.isEmpty() && !weatherPeriodSupported()
          ? tr("No cloud forecast for this date. Forecasts cover at most the "
               "next 16 days; the complete observing period must be covered.")
          : weatherStatus_;
  if (matches) {
    sample["received"] =
        localTime(weatherForecast_.value("receivedUtc").toLongLong());
    sample["gridLatitude"] = weatherForecast_.value("gridLatitude");
    sample["gridLongitude"] = weatherForecast_.value("gridLongitude");
  }
  if (sample.contains("utc"))
    sample["validTime"] = localTime(sample.value("utc").toLongLong());
  return sample;
}
QVariantMap ObservationController::recommendationWeather() const {
  QVariantMap result;
  if (!observationWeatherMatches(weatherForecast_, site_.latitude,
                                 site_.longitude))
    return result;
  for (const auto &value : recommendations_) {
    const auto recommendation = value.toMap();
    result[recommendation.value("id").toString()] = observationWeatherRange(
        weatherForecast_, recommendation.value("slots").toList());
  }
  return result;
}
void ObservationController::updateWeatherContext() {
  const QString key = QString::number(site_.latitude, 'g', 17) + ":" +
                      QString::number(site_.longitude, 'g', 17) + ":" +
                      QString::number(start_) + ":" + QString::number(end_) +
                      ":" + settings_.value("zone").toString();
  if (key == weatherContext_)
    return;
  weatherContext_ = key;
  weatherStatus_.clear();
  weatherFailed_ = false;
  ++weatherGeneration_;
  weatherBusy_ = false;
  if (weatherReply_)
    weatherReply_->abort();
  if (weatherEnabled_)
    weatherDebounce_.start();
  emit weatherChanged();
}
void ObservationController::refreshWeather(bool force) {
  weatherDebounce_.stop();
  if (weatherBusy_)
    return;
  if (!weatherPeriodSupported()) {
    weatherStatus_ =
        tr("No cloud forecast for this date. Forecasts cover at most the next "
           "16 days; the complete observing period must be covered.");
    emit weatherChanged();
    return;
  }
  weatherFailed_ = false;
  if (!force &&
      observationWeatherMatches(weatherForecast_, site_.latitude,
                                site_.longitude) &&
      QDateTime::currentSecsSinceEpoch() -
              weatherForecast_.value("receivedUtc").toLongLong() <=
          3600) {
    const auto series = weatherForecast_.value("samples").toList();
    if (!series.isEmpty() &&
        series.first().toMap().value("utc").toLongLong() <= start_ &&
        series.last().toMap().value("utc").toLongLong() >= end_) {
      weatherEnabled_ = true;
      weatherStatus_.clear();
      emit weatherChanged();
      return;
    }
  }
  if (offline_) {
    weatherFailed_ = true;
    weatherStatus_ = networkFailure(QNetworkReply::NetworkSessionFailedError,
                                    connectionLost_);
    emit weatherChanged();
    return;
  }
  weatherEnabled_ = true;
  if (!weatherNetwork_)
    weatherNetwork_ = new QNetworkAccessManager(this);
  const double latitude = site_.latitude, longitude = site_.longitude;
  QNetworkRequest request(observationWeatherUrl(latitude, longitude));
  request.setTransferTimeout(12000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = weatherNetwork_->get(request);
  weatherReply_ = reply;
  weatherBusy_ = true;
  weatherStatus_ = tr("Loading the hourly cloud forecast…");
  const int generation = ++weatherGeneration_;
  emit weatherChanged();
  connect(reply, &QNetworkReply::readyRead, reply, [reply] {
    if (reply->bytesAvailable() > 256 * 1024)
      reply->abort();
  });
  QTimer::singleShot(20000, reply, [reply] {
    if (!reply->isFinished())
      reply->abort();
  });
  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, generation, latitude, longitude] {
        if (generation != weatherGeneration_) {
          reply->deleteLater();
          return;
        }
        weatherBusy_ = false;
        weatherReply_ = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
          weatherFailed_ = true;
          weatherStatus_ = networkFailure(int(reply->error()), connectionLost_);
        } else {
          const auto data = reply->readAll();
          const auto received = QDateTime::currentSecsSinceEpoch();
          try {
            auto forecast =
                parseObservationWeather(data, latitude, longitude, received);
            weatherForecast_ = forecast;
            QSettings().setValue("observation/weatherCache",
                                 QVariantMap{{"data", data},
                                             {"latitude", latitude},
                                             {"longitude", longitude},
                                             {"receivedUtc", received}});
            weatherStatus_ = tr("Hourly forecast loaded. Percentages are model "
                                "predictions, not a live measurement.");
          } catch (const std::exception &e) {
            weatherFailed_ = true;
            weatherStatus_ = tr(e.what());
          }
        }
        reply->deleteLater();
        emit weatherChanged();
      });
}
} // namespace astro::app
