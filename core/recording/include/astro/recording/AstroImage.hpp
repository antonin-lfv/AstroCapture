#pragma once
#include <astro/domain/Frame.hpp>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
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
};
AstroImage readAstroImage(const std::filesystem::path& path, double inputScale = 0);
void writeAstroFits(const std::filesystem::path& path, const AstroImage& image);
AstroImage imageFromFrame(const domain::FrameBuffer& frame);
}
