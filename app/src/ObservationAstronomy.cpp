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
