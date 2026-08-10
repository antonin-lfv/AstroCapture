#pragma once

#include <QString>

namespace astro::app {

// Replaces anything but letters/digits/'-'/'_' with '_', so the result is
// safe as a path component on macOS/Windows/Linux (IMPLEMENTATION_BLUEPRINT.md
// section 17: "Sanitize for all target filesystems").
QString sanitizeForFilename(const QString& value);

// {date}_{time}_{camera} base name (blueprint section 17's naming pattern,
// simplified: no {target}/{sequence} tokens until sessions/multi-shot
// naming exist). Callers append their own extension.
QString timestampedBaseName(const QString& cameraId);

}  // namespace astro::app
