#pragma once
#include <astro/processing/DeepSkyAdvanced.hpp>
namespace astro::processing {
struct StarSeparationOptions {PsfOptions psf;int maximumStars{2000};double fitTolerance{.15},saturation{.98};};
struct StarLayerAdjustments {double starGain{1},starSaturation{1},starMidtones{.5},backgroundGain{1},backgroundSaturation{1},backgroundDenoise{};};
struct StarLayers {FloatImage image,background,stars;int detected{},accepted{},rejected{},saturated{};double fitRms{};};
// Circular PSF + local plane fits. Residual retains extended structures and all
// rejected sources; this is an inspectable estimate, not neural star removal.
StarLayers separateStars(const FloatImage&,const StarSeparationOptions&,const CheckCancel& = {});
// Subtract in the common display domain so neutral controls reproduce the common
// finish exactly, including nonlinear stretch. Float32 layers remain signed.
StarLayers finishStarLayers(const FloatImage&,const StarSeparationOptions&,const Finish&,const StarLayerAdjustments&,const CheckCancel& = {});
}
