#pragma once
#include <QVariantList>
#include <astro/recording/AstroImage.hpp>
#include <stop_token>
#include <astro/processing/DeepSky.hpp>
#include <functional>
namespace astro::app {
QVariantMap calibrationMetadata(const recording::AstroImage &);
// 0 compatible, 1 missing metadata (explicit review), 2 incompatible.
QVariantMap masterCompatibility(const QVariantMap &master,
                                const QVariantList &lights);
QVariantList calibrationMasterInventory();
QVariantMap calibrationMaster(const QString &id, std::stop_token = {});
QVariantMap storeCalibrationMaster(recording::AstroImage image,
                                   const QString &name, std::stop_token,
                                   const QVariantMap &provenance = {});
// Strict groups of selected raw calibration frames; ambiguity is reviewed before execution.
QVariantMap calibrationBatchPlan(const QVariantList &frames,const QVariantList &library = {});
QVariantMap calibrationCorrectionCompatibility(const QVariantMap &candidate,
                                               const QVariantMap &flatGroup);
recording::AstroImage buildCalibrationBatchMaster(
    const QVariantMap &group, const QVariantMap &correction,
    const std::function<recording::AstroImage(const QVariantMap &)> &load,
    std::stop_token, const processing::IntegrationLimits &);
void removeCalibrationMaster(const QString &id);
QString calibrationFileHash(const QString &, std::stop_token = {});
} // namespace astro::app
