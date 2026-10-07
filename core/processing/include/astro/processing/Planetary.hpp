#pragma once
#include <array>
#include <astro/domain/PixelFormat.hpp>
#include <functional>
#include <span>
#include <vector>

namespace astro::processing {
// Linear RGB, normalized to the source bit depth, never quantized for processing.
struct FloatImage {
    int width{}, height{};
    std::vector<float> rgb;
};
using CheckCancel = std::function<void()>;  // May throw to abandon a partial result.
FloatImage decodeSamples(std::span<const std::uint16_t> samples, int width, int height, int channels,
                         int bits, domain::BayerPattern bayer);
struct GrayImage {
    int width{}, height{};
    std::vector<float> pixels;
};
GrayImage luminance(const FloatImage& image, int maxSide = 256);
double qualityScore(const GrayImage& image);
// Sample native-resolution Sobel gradients: do not erase focus detail by shrinking first.
double frameQuality(const FloatImage& image);
struct Translation {
    double x{}, y{}, correlation{};
};
// Translation is a source sampling offset: result(x,y) = source(x+dx,y+dy).
Translation alignTranslation(const GrayImage& reference, const GrayImage& source,
                             const CheckCancel& check = {});
struct AlignmentReference {
    GrayImage coarse, detail;
    int width{}, height{}, sampleStep{}, originX{}, originY{};
};
AlignmentReference makeAlignmentReference(const FloatImage& image);
Translation alignToReference(const AlignmentReference& reference, const FloatImage& image,
                             const CheckCancel& check = {});
class MeanStack {
public:
    MeanStack(int width, int height);
    void add(const FloatImage& image, double dx, double dy, const CheckCancel& check = {});
    void restrictSupport(int left, int top, int right, int bottom);
    FloatImage finish() const;  // Crop to common support; no dark or underweighted borders.
    int count() const { return count_; }

private:
    int width_{}, height_{};
    std::vector<double> sum_;
    int left_{}, top_{}, right_{}, bottom_{}, count_{};
};
// Six undecimated B3-spline scales. Zero gains give the original exactly.
// Denoise soft-thresholds detail additions; RGB gains and gamma apply at the end.
FloatImage wavelets(const FloatImage& image, const std::array<float, 6>& gains, float denoise,
                    const std::array<float, 3>& color, float gamma, const CheckCancel& check = {});
}  // namespace astro::processing
