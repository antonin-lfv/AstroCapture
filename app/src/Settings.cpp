#include "Settings.hpp"

#include <QSettings>

#include "FileNaming.hpp"

namespace astro::app {

namespace {

QString exposureKey(const QString& cameraId) { return "cameraProfiles/" + cameraId + "/exposureMicroseconds"; }

QString gainKey(const QString& cameraId) { return "cameraProfiles/" + cameraId + "/gain"; }

QString presetsGroup(const QString& cameraId) { return "cameraProfiles/" + cameraId + "/presets"; }

QString presetGroup(const QString& cameraId, const QString& name) {
    return presetsGroup(cameraId) + QLatin1Char('/') + sanitizeForFilename(name);
}

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

void Settings::saveRootDirectory(const QString& path) {
    QSettings settings;
    settings.setValue(QStringLiteral("application/saveRootDirectory"), path);
}

std::optional<QString> Settings::loadRootDirectory() const {
    QSettings settings;
    const QString key = QStringLiteral("application/saveRootDirectory");
    if (!settings.contains(key)) {
        return std::nullopt;
    }
    const QString value = settings.value(key).toString();
    return value.isEmpty() ? std::nullopt : std::make_optional(value);
}

void Settings::savePreset(const QString& cameraId, const CameraPreset& preset) {
    QSettings settings;
    const QString group = presetGroup(cameraId, preset.name);
    settings.setValue(group + QStringLiteral("/name"), preset.name);
    settings.setValue(group + QStringLiteral("/exposureMicroseconds"), preset.exposureMicroseconds);
    settings.setValue(group + QStringLiteral("/gain"), preset.gain);
}

void Settings::deletePreset(const QString& cameraId, const QString& name) {
    QSettings settings;
    settings.remove(presetGroup(cameraId, name));
}

QStringList Settings::presetNames(const QString& cameraId) const {
    QSettings settings;
    settings.beginGroup(presetsGroup(cameraId));
    const QStringList sanitizedNames = settings.childGroups();
    settings.endGroup();

    // Group keys are the sanitized name; the display name is stored inside
    // each group so it round-trips even if sanitization changed it.
    QStringList names;
    names.reserve(sanitizedNames.size());
    for (const QString& sanitized : sanitizedNames) {
        const QString storedName =
            settings.value(presetsGroup(cameraId) + QLatin1Char('/') + sanitized + QStringLiteral("/name"))
                .toString();
        names.append(storedName.isEmpty() ? sanitized : storedName);
    }
    return names;
}

std::optional<CameraPreset> Settings::loadPreset(const QString& cameraId, const QString& name) const {
    QSettings settings;
    const QString group = presetGroup(cameraId, name);
    const QString nameKey = group + QStringLiteral("/name");
    if (!settings.contains(nameKey)) {
        return std::nullopt;
    }

    CameraPreset preset;
    preset.name = settings.value(nameKey).toString();
    preset.exposureMicroseconds = settings.value(group + QStringLiteral("/exposureMicroseconds")).toDouble();
    preset.gain = settings.value(group + QStringLiteral("/gain")).toLongLong();
    return preset;
}

}  // namespace astro::app
