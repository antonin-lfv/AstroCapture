#pragma once
#include "Planetary.hpp"
#include <memory>
namespace astro::processing {
struct SurfaceWarp {
    FloatImage image;
    int left{}, top{}, right{}, bottom{};
    int tracked{}, candidates{};
};
// Local normalized template matching around an already registered frame.
// Unreliable/untextured patches fall back to the global translation.
class SurfaceReference {
public:
    SurfaceReference(const FloatImage& image, int patchSize = 96);
    int areaCount() const;
    std::vector<float> qualities(const FloatImage& image, Translation global, const CheckCancel& check = {}) const;
    std::vector<float> selectionWeights(std::span<const float> areaWeights) const;
    SurfaceWarp align(const FloatImage& image, Translation global, const CheckCancel& check = {}) const;
private:
    struct Impl;
    std::shared_ptr<const Impl> impl_;
};
struct LocalStackResult {
    FloatImage image;
    int left{}, top{};
    int usedFrames{}, rejectedFrames{}, correctedFrames{}, areas{}, framesPerArea{};
};
// Input indices are globally ranked best first. Every reliable frame is scored
// in every area; each area retains its own best K frames. Only scores and
// translations are retained between the two streaming passes.
LocalStackResult stackLocalSelection(const std::vector<int>& indices, int keepCount,
    const std::function<FloatImage(int)>& load, int patchSize = 96,
    const CheckCancel& check = {}, const std::function<void(int,int)>& progress = {});
}
