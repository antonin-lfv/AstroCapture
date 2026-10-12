#pragma once
#include <QByteArray>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
namespace astro::app {
QUrl observationWeatherUrl(double latitude, double longitude);
QVariantMap parseObservationWeather(const QByteArray &, double latitude,
                                    double longitude, qint64 receivedUtc);
bool observationWeatherMatches(const QVariantMap &, double latitude,
                               double longitude);
QVariantMap observationWeatherAt(const QVariantMap &, qint64 utc);
QVariantMap observationWeatherRange(const QVariantMap &,
                                    const QVariantList &windows);
} // namespace astro::app
