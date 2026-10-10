#pragma once
#include <QByteArray>
#include <QUrl>
#include <QVariantMap>
#include <stop_token>
namespace astro::app {
QUrl targetResolverUrl(const QString &name);
QVariantMap parseTargetPosition(const QByteArray &, const QString &query);
QVariantMap resolveTargetPosition(const QString &name, std::stop_token);
QString rightAscensionText(double degrees);
QString declinationText(double degrees);
} // namespace astro::app
