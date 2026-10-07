#include <astro/processing/Planetary.hpp>
#include <cmath>
#include <random>
#include <stdexcept>

#include "Check.hpp"
using namespace astro::processing;
using astro::domain::BayerPattern;
namespace {
float signal(double x, double y) {
    return float(.1 + .35 * std::exp(-((x - 37) * (x - 37) + (y - 29) * (y - 29)) / 120) +
                 .3 * std::exp(-((x - 85) * (x - 85) + (y - 63) * (y - 63)) / 210) +
                 .12 * std::sin(x * .24) * std::cos(y * .16));
}
FloatImage image(double dx = 0, double dy = 0) {
    FloatImage out{128, 96, std::vector<float>(128 * 96 * 3)};
    for (int y = 0; y < 96; ++y)
        for (int x = 0; x < 128; ++x)
            for (int c = 0; c < 3; ++c) out.rgb[(y * 128 + x) * 3 + c] = signal(x - dx, y - dy);
    return out;
}
}  // namespace
int main() {
    // All four mosaics, edges included; retain real 12-bit precision.
    for (auto pattern : {BayerPattern::RGGB, BayerPattern::BGGR, BayerPattern::GRBG, BayerPattern::GBRG}) {
        std::vector<std::uint16_t> samples(7 * 5);
        const int colors[3] = {3501, 1733, 217};
        for (int y = 0; y < 5; ++y)
            for (int x = 0; x < 7; ++x)
                samples[y * 7 + x] = colors[astro::domain::bayerChannelAt(pattern, x, y)];
        auto out = decodeSamples(samples, 7, 5, 1, 12, pattern);
        for (std::size_t i = 0; i < out.rgb.size(); ++i)
            ASTRO_TEST_CHECK(std::abs(out.rgb[i] - colors[i % 3] / 4095.f) < 1e-6);
    }
    auto reference = image();
    const auto gray = luminance(reference);
    for (auto shift :
         {std::array<double, 2>{5, -3}, std::array<double, 2>{-7, 4}, std::array<double, 2>{2.35, -1.7}}) {
        const auto offset = alignTranslation(gray, luminance(image(shift[0], shift[1])));
        ASTRO_TEST_CHECK(offset.correlation > .98);
        ASTRO_TEST_CHECK(std::abs(offset.x - shift[0]) < .15 && std::abs(offset.y - shift[1]) < .15);
    }
    // Native 5 MP registration must retain sub-pixel accuracy after coarse downsampling.
    {
        const auto large = [](double dx, double dy) {
            FloatImage out{2592, 1944, std::vector<float>(2592 * 1944 * 3)};
            for (int y = 0; y < out.height; ++y)
                for (int x = 0; x < out.width; ++x) {
                    const double xx = x - dx, yy = y - dy;
                    const float v =
                        signal(xx / 20, yy / 20) + float(.018 * std::sin(xx * .39) * std::cos(yy * .31));
                    for (int c = 0; c < 3; ++c) out.rgb[(std::size_t(y) * out.width + x) * 3 + c] = v;
                }
            return out;
        };
        const auto full = large(0, 0);
        const auto target = makeAlignmentReference(full);
        const auto offset = alignToReference(target, large(3.35, -2.6));
        ASTRO_TEST_CHECK(offset.correlation > .9);
        ASTRO_TEST_CHECK(std::abs(offset.x - 3.35) < .2 && std::abs(offset.y + 2.6) < .2);
    }
    auto flat = reference;
    std::fill(flat.rgb.begin(), flat.rgb.end(), .3f);
    ASTRO_TEST_CHECK(qualityScore(luminance(flat)) < 1e-12);
    ASTRO_TEST_CHECK(alignTranslation(gray, luminance(flat)).correlation < .5);
    auto blurred = reference;
    for (int y = 4; y < 92; ++y)
        for (int x = 4; x < 124; ++x)
            for (int c = 0; c < 3; ++c) {
                double v = 0;
                for (int j = -4; j <= 4; ++j)
                    for (int i = -4; i <= 4; ++i) v += reference.rgb[((y + j) * 128 + x + i) * 3 + c];
                blurred.rgb[(y * 128 + x) * 3 + c] = float(v / 81);
            }
    ASTRO_TEST_CHECK(qualityScore(gray) > qualityScore(luminance(blurred)));
    ASTRO_TEST_CHECK(frameQuality(reference) > frameQuality(blurred));
    ASTRO_TEST_CHECK(frameQuality(flat) < 1e-12);
    MeanStack aligned(128, 96);
    aligned.add(reference, 0, 0);
    aligned.add(image(5, -3), 5, -3);
    const auto combined = aligned.finish();
    ASTRO_TEST_CHECK(combined.width == 123 && combined.height == 93);
    for (int y = 0; y < combined.height; ++y)
        for (int x = 0; x < combined.width; ++x)
            ASTRO_TEST_CHECK(std::abs(combined.rgb[(y * combined.width + x) * 3] - signal(x, y + 3)) < 1e-6);
    // Independent noise averages down, and the stack keeps fractional precision.
    std::mt19937 random(77);
    std::normal_distribution<float> noise(0, .04f);
    MeanStack noisy(128, 96);
    double firstError = 0;
    for (int n = 0; n < 16; ++n) {
        auto frame = reference;
        for (std::size_t i = 0; i < frame.rgb.size(); ++i) {
            frame.rgb[i] += noise(random);
            if (!n) firstError += std::pow(frame.rgb[i] - reference.rgb[i], 2);
        }
        noisy.add(frame, 0, 0);
    }
    const auto averaged = noisy.finish();
    double averagedError = 0;
    for (std::size_t i = 0; i < averaged.rgb.size(); ++i)
        averagedError += std::pow(averaged.rgb[i] - reference.rgb[i], 2);
    ASTRO_TEST_CHECK(averagedError < firstError / 10);
    const auto identity = wavelets(flat, {0, 0, 0, 0, 0, 0}, 0, {1, 1, 1}, 1);
    ASTRO_TEST_CHECK(identity.rgb == flat.rgb);
    const auto sharpenedFlat = wavelets(flat, {2, 2, 2, 2, 2, 2}, 0, {1, 1, 1}, 1);
    for (std::size_t i = 0; i < flat.rgb.size(); ++i)
        ASTRO_TEST_CHECK(std::abs(sharpenedFlat.rgb[i] - flat.rgb[i]) < 1e-6);
    const auto sharpened = wavelets(reference, {1, 1, 0, 0, 0, 0}, 0, {1, 1, 1}, 1);
    ASTRO_TEST_CHECK(qualityScore(luminance(sharpened)) > qualityScore(gray));
    bool cancelled = false;
    try {
        wavelets(reference, {1, 1, 1, 1, 1, 1}, 0, {1, 1, 1}, 1, [] { throw std::runtime_error("cancel"); });
    } catch (...) {
        cancelled = true;
    }
    ASTRO_TEST_CHECK(cancelled);
}
