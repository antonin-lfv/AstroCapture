#pragma once
#include <QDateTime>
#include <QString>
#include <vector>
namespace astro::app {
struct ObservationTarget {
  QString id, name, source;
  int body{-1};
  double ra{}, dec{};
};
struct ObservationSite {
  double latitude{}, longitude{}, elevation{};
};
struct ObservationPosition {
  double altitude{}, azimuth{}, ra{}, dec{};
};
struct ObservationSample {
  qint64 utc{};
  double altitude{}, azimuth{}, sun{};
};
struct ObservationWindow {
  qint64 start{}, end{};
};
struct ObservationSolarEvent {
  qint64 utc{};
  QString kind;
};
std::vector<ObservationSolarEvent>
observationSolarEvents(const ObservationSite &, qint64 start, qint64 end);
struct ObservationSunPosition {
  double altitude{};
  bool isUp{};
};
ObservationSunPosition observationSunPosition(const ObservationSite &, qint64 utc);
bool observationSunIsUp(const ObservationSite &, qint64 utc);
ObservationPosition observationPosition(const ObservationTarget &,
                                        const ObservationSite &, qint64 utc);
double observationSunAltitude(const ObservationSite &, qint64 utc);
double observationMoonFraction(qint64 utc);
double observationSeparation(const ObservationPosition &,
                             const ObservationPosition &);
std::vector<ObservationSample> observationSamples(const ObservationTarget &,
                                                  const ObservationSite &,
                                                  qint64 start, qint64 end);
std::vector<ObservationWindow>
observationWindows(const ObservationTarget &, const ObservationSite &,
                   qint64 start, qint64 end, double minimum, double sunLimit);
QDateTime observationNoon(const QString &date, const QString &zone);
qint64 observationNightEnd(const ObservationSite &, const QString &date,
                           const QString &zone);
QString observationPlanningDate(const ObservationSite &, const QString &zone,
                                qint64 now);
} // namespace astro::app
