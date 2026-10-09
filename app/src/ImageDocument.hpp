#pragma once
#include <astro/processing/Planetary.hpp>
#include <QImage>
#include <QVariantMap>
#include <QUrl>
#include <QMetaType>
#include <memory>
namespace astro::app {
// Immutable, full-precision image handoff; the caption remains editable.
struct ImageDocument {
    std::shared_ptr<const processing::FloatImage> pixels;
    QImage preview;
    QVariantMap annotation;
    QString name;
    QUrl exportSuggestion;
    QStringList protectedPaths;
    std::shared_ptr<const std::vector<float>> edgeMask;
    int mosaicImageCount{};
    QString processingSummary;
    int exposureCount{};
    double totalExposure{};
    QStringList filters;
};
struct ExportSizeInput {
    std::shared_ptr<const processing::FloatImage> pixels;
    QImage overlay;
};
}
Q_DECLARE_METATYPE(astro::app::ImageDocument)
Q_DECLARE_METATYPE(astro::app::ExportSizeInput)
