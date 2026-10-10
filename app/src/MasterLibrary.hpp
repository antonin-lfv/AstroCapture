#pragma once
#include <QVariantList>
#include <astro/recording/AstroImage.hpp>
#include <stop_token>
namespace astro::app {
QVariantMap calibrationMetadata(const recording::AstroImage&);
// 0 compatible, 1 missing metadata (explicit review), 2 incompatible.
QVariantMap masterCompatibility(const QVariantMap& master,const QVariantList& lights);
QVariantList calibrationMasterInventory();
QVariantMap calibrationMaster(const QString& id,std::stop_token = {});
QVariantMap storeCalibrationMaster(recording::AstroImage image,const QString& name,std::stop_token);
void removeCalibrationMaster(const QString& id);
QString calibrationFileHash(const QString&,std::stop_token = {});
}
