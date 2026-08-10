#pragma once

#include <optional>

#include <QString>

namespace astro::app {

// Thin QSettings wrapper for the camera profile persisted so far: last-used
// exposure/gain per camera id, namespaced as cameraProfiles/<id>/... per
// docs/development/IMPLEMENTATION_BLUEPRINT.md section 16. Assumes camera
// ids don't contain '/', true for every backend today (mock's static id,
// AVFoundation's uniqueID).
class Settings {
public:
    void saveExposureMicroseconds(const QString& cameraId, double value);
    void saveGain(const QString& cameraId, qint64 value);

    std::optional<double> loadExposureMicroseconds(const QString& cameraId) const;
    std::optional<qint64> loadGain(const QString& cameraId) const;
};

}  // namespace astro::app
