#pragma once
#include <QVariantMap>
#include <QString>
#include <astro/processing/DeepSky.hpp>
#include <stop_token>
namespace astro::app {
QVariantMap blindSolverConfiguration();
void setBlindIndexDirectory(const QString&);
void installWideFieldIndex(std::stop_token);
recording::CelestialWcs solveBlindField(const processing::FloatImage&, const QVariantMap&, std::stop_token);
}
