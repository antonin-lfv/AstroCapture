#pragma once
#include <QByteArray>
#include <QMap>
#include <QStringList>
#include <QVariantList>
#include <astro/processing/Astrometry.hpp>
namespace astro::app {
std::vector<processing::CatalogStar> parseGaiaCatalog(const QByteArray &,
                                                      double epoch);
processing::Spectrum parseGaiaSpectrum(const QByteArray &,
                                       const QString &sourceId);
QMap<QString, processing::Spectrum>
parseGaiaSpectraArchive(const QByteArray &, const QStringList &ids);
QVariantList catalogValues(const std::vector<processing::CatalogStar> &);
std::vector<processing::CatalogStar> readCatalogValues(const QVariantList &);
QByteArray linearImageKey(const processing::FloatImage &);
} // namespace astro::app
