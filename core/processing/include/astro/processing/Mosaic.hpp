#pragma once
#include "Planetary.hpp"
#include <string>
namespace astro::processing {
struct MosaicTile {
    FloatImage image;
    std::vector<std::uint8_t> mask; // empty = opaque; zero = missing/transparent
};
struct MosaicSource { std::string name; int width{}, height{}; };
struct MosaicResult {
    FloatImage image;
    int pairs{}, matches{};
    double residual{};
    std::vector<int> usedIndices, excludedIndices;
    // Smooth support mask, empty when edge cleanup is disabled.
    std::vector<float> edgeMask;
};
using LoadMosaicTile = std::function<MosaicTile(int)>;
using MosaicProgress = std::function<void(int, const std::string&)>;
// Similarity registration (translation, rotation, uniform scale), global least
// squares, scalar exposure matching and feathered blending in floating point.
// Optional rejection retains the largest connected group and reports original indices.
MosaicResult buildMosaic(const std::vector<MosaicSource>& sources, const LoadMosaicTile& load,
                        bool balanceExposure = true, const CheckCancel& check = {},
                        const MosaicProgress& progress = {}, bool skipUnmatched = false, bool softenEdges = false);
}
