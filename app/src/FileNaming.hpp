#pragma once

#include <QString>

namespace astro::app {

// Replaces anything but letters/digits/'-'/'_' with '_', so the result is
// safe as a path component on macOS/Windows/Linux (IMPLEMENTATION_BLUEPRINT.md
// section 17: "Sanitize for all target filesystems").
QString sanitizeForFilename(const QString& value);

// {date}_{time}_{target}_{camera} base name (blueprint section 17's naming
// pattern). `targetName` is
// omitted entirely when empty, so existing filenames without a target
// still look the same. Callers append their own extension.
QString timestampedBaseName(const QString& cameraId, const QString& targetName = QString());

// Reserve a new empty file exclusively, adding a numeric suffix on
// collision. Returns empty on failure. Writers may overwrite this file,
// which belongs to this capture, without truncating a previous capture.
QString reserveCapturePath(const QString& directory, const QString& baseName, const QString& extension);

}  // namespace astro::app
