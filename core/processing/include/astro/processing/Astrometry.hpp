#pragma once
#include <astro/processing/DeepSkyAdvanced.hpp>
#include <string>
namespace astro::processing {
struct CatalogStar {
  std::string id;
  double ra{}, dec{}, magnitude{}, temperature{};
  bool spectrum{};
};
struct AstrometricMatch {
  int detected{}, catalog{};
  double x{}, y{}, residual{};
};
struct AstrometricSolution {
  recording::CelestialWcs wcs;
  std::vector<AstrometricMatch> matches;
  double rms{}, coverage{};
};
// Targeted TAN solver: the seed supplies approximate field centre and scale.
// Rotation and parity are inferred; no pixels are modified.
AstrometricSolution solveAstrometry(const FloatImage &,
                                    const std::vector<CatalogStar> &,
                                    const recording::CelestialWcs &seed,
                                    const CheckCancel & = {});
struct ResponseSample {
  double wavelength{};
  std::array<double, 3> response{};
};
using SpectralResponse = std::vector<ResponseSample>;
using Spectrum = std::vector<std::array<double, 2>>; // nm, energy flux per nm
std::array<double, 3> spectralFlux(const Spectrum &, const SpectralResponse &);
Spectrum temperatureSpectrum(double kelvin); // PCC continuum model, not Gaia XP
SpectralResponse
standardResponse();       // Bessell R/V/B reference, not a camera model
Spectrum solarSpectrum(); // CALSPEC sun_reference_stis_002, resampled in nm
std::array<double, 3> referenceColors(const Spectrum &,
                                      const SpectralResponse &,
                                      const Spectrum &white);
} // namespace astro::processing
