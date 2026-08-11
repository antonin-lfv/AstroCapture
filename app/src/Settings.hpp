#pragma once

#include <optional>

#include <QStringList>

namespace astro::app {

struct CameraPreset {
    QString name;
    double exposureMicroseconds = 0.0;
    qint64 gain = 0;
};

// Thin QSettings wrapper for AstroCapture's persisted state, namespaced per
// docs/development/IMPLEMENTATION_BLUEPRINT.md section 16:
//   application/saveRootDirectory
//   cameraProfiles/<id>/exposureMicroseconds, .../gain
//   cameraProfiles/<id>/presets/<name>/exposureMicroseconds, .../gain
// Assumes camera ids and preset names don't contain '/': true for every
// camera id today (mock's static id, AVFoundation's uniqueID), and enforced
// for preset names via FileNaming's sanitizer since they're user-typed.
class Settings {
public:
    void saveExposureMicroseconds(const QString& cameraId, double value);
    void saveGain(const QString& cameraId, qint64 value);
    std::optional<double> loadExposureMicroseconds(const QString& cameraId) const;
    std::optional<qint64> loadGain(const QString& cameraId) const;

    // Generic per-camera scalar control storage (Gate D backlog 22.3:
    // gamma/white balance/brightness/frame rate limit), keyed by
    // PreviewController::ExtraControl::key. Not part of named presets —
    // exposure/gain remain the only preset-scoped fields for now.
    void saveExtraControl(const QString& cameraId, const QString& key, double value);
    std::optional<double> loadExtraControl(const QString& cameraId, const QString& key) const;

    void saveRootDirectory(const QString& path);
    std::optional<QString> loadRootDirectory() const;

    void savePreset(const QString& cameraId, const CameraPreset& preset);
    void deletePreset(const QString& cameraId, const QString& name);
    QStringList presetNames(const QString& cameraId) const;
    std::optional<CameraPreset> loadPreset(const QString& cameraId, const QString& name) const;
};

}  // namespace astro::app
