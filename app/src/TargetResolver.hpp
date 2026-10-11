#pragma once
#include <QByteArray>
#include <QUrl>
#include <QVariantMap>
#include <stdexcept>
#include <stop_token>
namespace astro::app {
class TargetResolverNetworkError : public std::runtime_error {
public:
  explicit TargetResolverNetworkError(int code)
      : std::runtime_error("The CDS target resolver is unavailable. Retry or "
                           "enter coordinates manually."),
        code(code) {}
  int code;
};
QUrl targetResolverUrl(const QString &name);
QVariantMap parseTargetPosition(const QByteArray &, const QString &query);
QVariantMap resolveTargetPosition(const QString &name, std::stop_token);
QString rightAscensionText(double degrees);
QString declinationText(double degrees);
} // namespace astro::app
