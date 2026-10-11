#pragma once
#include <astro/domain/Frame.hpp>
#include <astro/recording/CelestialWcs.hpp>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <span>
#include <functional>
namespace astro::recording {
// Interleaved, linear samples. Integer FITS use physical values after BZERO/BSCALE.
// Calibration operates on these samples before demosaicing. No display stretch.
struct AstroImage {
    int width{}, height{}, channels{1}, bits{};
    domain::BayerPattern bayer{domain::BayerPattern::None};
    double exposure{}, gain{}, temperature{};
    bool hasGain{}, hasTemperature{};
    double scale{1}; // original ADU units per normalized sample
    std::string filter, object, date, imageType;
    std::vector<float> samples;
    int fitsBitpix{}; // Original FITS storage type; negative means floating point.
    std::optional<CelestialWcs> wcs;
    std::optional<double> centerRa,centerDec,pixelScale;
    std::string camera, masterRole;
    int binX{}, binY{}, masterCount{};
    double offset{}, originalScale{1};
    bool hasOffset{}, normalizedMasterFlat{};
    std::uint64_t masterRejected{};
};
AstroImage readAstroImage(const std::filesystem::path& path, double inputScale = 0);
std::optional<CelestialWcs> readCelestialWcs(const std::filesystem::path& path);
void writeAstroFits(const std::filesystem::path& path, const AstroImage& image);
// Metadata can be separate from immutable full-precision pixels. Writes planar
// FITS in bounded blocks, without duplicating the scientific image.
void writeAstroFits(const std::filesystem::path&,const AstroImage& metadata,std::span<const float>,const std::function<void()>& check = {});
AstroImage imageFromFrame(const domain::FrameBuffer& frame);
}
