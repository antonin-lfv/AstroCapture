#pragma once
#include <QImage>
#include <QMap>
#include <QVariantMap>
#include <astro/processing/Planetary.hpp>
#include <memory>

namespace astro::app {
// Captured on the GUI thread. Immutable pixels can then be written without
// blocking editing or copying image buffers into parameter history.
struct WorkspaceSnapshot {
    QVariantMap state;
    QMap<QString, std::shared_ptr<const processing::FloatImage>> images;
    QMap<QString, std::shared_ptr<const std::vector<float>>> planes;
    QMap<QString, QImage> previews;
};
}
