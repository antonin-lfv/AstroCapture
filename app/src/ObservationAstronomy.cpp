#include "ObservationAstronomy.hpp"
#include "astronomy.h"
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace astro::app {
namespace {
astro_time_t timeAt(qint64 utc) {
  return Astronomy_TimeFromDays((utc - 946728000) / 86400.0);
}
void valid(const ObservationSite &s, qint64 utc) {
  const auto year =
      QDateTime::fromSecsSinceEpoch(utc, QTimeZone::UTC).date().year();
  if (!std::isfinite(s.latitude) || !std::isfinite(s.longitude) ||
      !std::isfinite(s.elevation) || std::abs(s.latitude) > 90 ||
      std::abs(s.longitude) > 180 || s.elevation < -500 ||
      s.elevation > 10000 || year < 1900 || year > 2100)
    throw std::runtime_error("Invalid observation site or date (1900–2100).");
}
} // namespace
ObservationPosition observationPosition(const ObservationTarget &target,
                                        const ObservationSite &site,
                                        qint64 utc) {
  valid(site, utc);
  auto time = timeAt(utc);
  auto observer =
      Astronomy_MakeObserver(site.latitude, site.longitude, site.elevation);
  astro_equatorial_t eq{};
  if (target.body >= 0) {
    eq = Astronomy_Equator(static_cast<astro_body_t>(target.body), &time,
                           observer, EQUATOR_OF_DATE, ABERRATION);
  } else {
    if (!std::isfinite(target.ra) || !std::isfinite(target.dec) ||
        target.ra < 0 || target.ra >= 360 || std::abs(target.dec) > 90)
      throw std::runtime_error("Invalid J2000 coordinates.");
    astro_spherical_t sphere{ASTRO_SUCCESS, target.dec, target.ra, 1.0};
    const auto j2000 = Astronomy_VectorFromSphere(sphere, time);
    eq = Astronomy_EquatorFromVector(
        Astronomy_RotateVector(Astronomy_Rotation_EQJ_EQD(&time), j2000));
  }
  if (eq.status != ASTRO_SUCCESS)
    throw std::runtime_error("Cannot calculate this object's position.");
  const auto horizontal =
      Astronomy_Horizon(&time, observer, eq.ra, eq.dec, REFRACTION_NONE);
  return {horizontal.altitude, horizontal.azimuth, eq.ra * 15, eq.dec};
}
double observationSunAltitude(const ObservationSite &site, qint64 utc) {
  return observationPosition({{}, {}, {}, BODY_SUN}, site, utc).altitude;
}
bool observationSunIsUp(const ObservationSite &site, qint64 utc) {
  valid(site, utc);
  auto time = timeAt(utc);
  auto observer =
      Astronomy_MakeObserver(site.latitude, site.longitude, site.elevation);
  auto eq =
      Astronomy_Equator(BODY_SUN, &time, observer, EQUATOR_OF_DATE, ABERRATION);
  if (eq.status != ASTRO_SUCCESS)
    throw std::runtime_error("Cannot calculate solar events.");
  auto atmosphere = Astronomy_Atmosphere(site.elevation);
  return Astronomy_Horizon(&time, observer, eq.ra, eq.dec, REFRACTION_NONE)
                 .altitude +
             (34.0 / 60) * atmosphere.density +
             std::asin(0.004650467260962157 / eq.dist) * RAD2DEG >=
         0;
}
std::vector<ObservationSolarEvent>
observationSolarEvents(const ObservationSite &site, qint64 start, qint64 end) {
  valid(site, start);
  if (end <= start || end - start > 26 * 3600)
    throw std::runtime_error("Invalid observation period.");
  auto observer =
      Astronomy_MakeObserver(site.latitude, site.longitude, site.elevation);
  std::vector<ObservationSolarEvent> events;
  for (const double altitude : {0.0, -6.0, -18.0}) {
    for (auto direction : {DIRECTION_RISE, DIRECTION_SET}) {
      qint64 cursor = start;
      // The local-noon period can contain two events of one type at extreme
      // latitudes.
      while (cursor < end) {
        auto result = altitude == 0
                          ? Astronomy_SearchRiseSet(
                                BODY_SUN, observer, direction, timeAt(cursor),
                                double(end - cursor) / 86400)
                          : Astronomy_SearchAltitude(
                                BODY_SUN, observer, direction, timeAt(cursor),
                                double(end - cursor) / 86400, altitude);
        if (result.status == ASTRO_SEARCH_FAILURE)
          break;
        if (result.status != ASTRO_SUCCESS)
          throw std::runtime_error("Cannot calculate solar events.");
        const qint64 utc = qRound64(result.time.ut * 86400 + 946728000);
        if (utc >= end)
          break;
        if (utc < cursor)
          break;
        QString kind =
            altitude == 0 ? (direction == DIRECTION_RISE ? "sunrise" : "sunset")
            : altitude == -6
                ? (direction == DIRECTION_RISE ? "civilDawn" : "civilDusk")
                : (direction == DIRECTION_RISE ? "nightEnd" : "nightStart");
        events.push_back({utc, kind});
        cursor = utc + 2;
      }
    }
  }
  std::sort(events.begin(), events.end(),
            [](const auto &a, const auto &b) { return a.utc < b.utc; });
  return events;
}
double observationMoonFraction(qint64 utc) {
  const auto result = Astronomy_Illumination(BODY_MOON, timeAt(utc));
  if (result.status != ASTRO_SUCCESS)
    throw std::runtime_error("Cannot calculate lunar phase.");
  return result.phase_fraction;
}
double observationSeparation(const ObservationPosition &a,
                             const ObservationPosition &b) {
  const double dot = std::sin(a.dec * DEG2RAD) * std::sin(b.dec * DEG2RAD) +
                     std::cos(a.dec * DEG2RAD) * std::cos(b.dec * DEG2RAD) *
                         std::cos((a.ra - b.ra) * DEG2RAD);
  return std::acos(std::clamp(dot, -1.0, 1.0)) * RAD2DEG;
}
QDateTime observationNoon(const QString &date, const QString &zone) {
  const auto d = QDate::fromString(date, Qt::ISODate);
  const QTimeZone z(zone.toUtf8());
  if (!d.isValid() || d.year() < 1900 || d.year() > 2100 || !z.isValid())
    throw std::runtime_error("Enter a valid date (1900–2100) and time zone.");
  return QDateTime(d, QTime(12, 0), z);
}
qint64 observationNightEnd(const ObservationSite &site, const QString &date,
                           const QString &zone) {
  const auto start = observationNoon(date, zone);
  const auto end =
      QDateTime(start.date().addDays(1), QTime(12, 0), start.timeZone())
          .toSecsSinceEpoch();
  // The last interval with the Sun below the horizon is the chosen night.
  // Polar nights and periods without a sunset keep the local-noon boundary.
  bool night = !observationSunIsUp(site, start.toSecsSinceEpoch());
  qint64 nightEnd = end;
  for (const auto &event :
       observationSolarEvents(site, start.toSecsSinceEpoch(), end)) {
    if (event.kind == "sunset") {
      night = true;
      nightEnd = end;
    } else if (event.kind == "sunrise" && night) {
      night = false;
      nightEnd = event.utc;
    }
  }
  return nightEnd;
}
QString observationPlanningDate(const ObservationSite &site,
                                const QString &zone, qint64 now) {
  const QTimeZone timeZone(zone.toUtf8());
  if (!timeZone.isValid())
    throw std::runtime_error("Enter a valid date (1900–2100) and time zone.");
  const auto local = QDateTime::fromSecsSinceEpoch(now, timeZone);
  const auto previous = local.date().addDays(-1).toString(Qt::ISODate);
  if (local.time() < QTime(12, 0) &&
      observationNightEnd(site, previous, zone) > now)
    return previous;
  return local.date().toString(Qt::ISODate);
}
std::vector<ObservationSample> observationSamples(const ObservationTarget &t,
                                                  const ObservationSite &s,
                                                  qint64 start, qint64 end) {
  if (end <= start || end - start > 26 * 3600)
    throw std::runtime_error("Invalid observation period.");
  std::vector<ObservationSample> samples;
  for (qint64 utc = start;; utc = std::min(utc + 300, end)) {
    const auto p = observationPosition(t, s, utc);
    samples.push_back(
        {utc, p.altitude, p.azimuth, observationSunAltitude(s, utc)});
    if (utc == end)
      break;
  }
  return samples;
}
std::vector<ObservationWindow>
observationWindows(const ObservationTarget &t, const ObservationSite &s,
                   qint64 start, qint64 end, double minimum, double sunLimit) {
  if (!std::isfinite(minimum) || minimum < 0 || minimum > 90 ||
      !std::isfinite(sunLimit) || sunLimit < -18 || sunLimit > 90 ||
      end <= start || end - start > 26 * 3600)
    throw std::runtime_error("Invalid visibility thresholds.");
  // One-minute bracketing, then one-second bisection of both constraints.
  auto margin = [&](qint64 utc) {
    return std::min(observationPosition(t, s, utc).altitude - minimum,
                    sunLimit - observationSunAltitude(s, utc));
  };
  std::vector<ObservationWindow> result;
  bool inside = margin(start) >= 0;
  qint64 begin = start, previous = start;
  for (qint64 utc = std::min(start + 60, end);; utc = std::min(utc + 60, end)) {
    const bool now = margin(utc) >= 0;
    if (now != inside) {
      qint64 a = previous, b = utc;
      while (b - a > 1) {
        auto mid = (a + b) / 2;
        if ((margin(mid) >= 0) == inside)
          a = mid;
        else
          b = mid;
      }
      if (now)
        begin = b;
      else
        result.push_back({begin, b});
      inside = now;
    }
    previous = utc;
    if (utc == end)
      break;
  }
  if (inside)
    result.push_back({begin, end});
  return result;
}
} // namespace astro::app
