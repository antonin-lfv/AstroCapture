#include "Settings.hpp"

#include <QSettings>

namespace astro::app {

namespace {

QString exposureKey(const QString& cameraId) { return "cameraProfiles/" + cameraId + "/exposureMicroseconds"; }

QString gainKey(const QString& cameraId) { return "cameraProfiles/" + cameraId + "/gain"; }

}  // namespace

void Settings::saveExposureMicroseconds(const QString& cameraId, double value) {
    QSettings settings;
    settings.setValue(exposureKey(cameraId), value);
}

void Settings::saveGain(const QString& cameraId, qint64 value) {
    QSettings settings;
    settings.setValue(gainKey(cameraId), value);
}

std::optional<double> Settings::loadExposureMicroseconds(const QString& cameraId) const {
    QSettings settings;
    const QString key = exposureKey(cameraId);
    if (!settings.contains(key)) {
        return std::nullopt;
    }
    return settings.value(key).toDouble();
}

std::optional<qint64> Settings::loadGain(const QString& cameraId) const {
    QSettings settings;
    const QString key = gainKey(cameraId);
    if (!settings.contains(key)) {
        return std::nullopt;
    }
    return settings.value(key).toLongLong();
}

}  // namespace astro::app
