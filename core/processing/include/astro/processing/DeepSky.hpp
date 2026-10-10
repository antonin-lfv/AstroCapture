#pragma once
#include <astro/processing/Planetary.hpp>
#include <astro/processing/Preview.hpp>
#include <astro/recording/AstroImage.hpp>
#include <astro/processing/IntegrationResources.hpp>
#include <functional>
#include <optional>
namespace astro::processing {
using recording::AstroImage;
using ImageLoader = std::function<AstroImage(int)>;
using Progress = std::function<void(int,int,const std::string&)>;
struct Calibration {
    std::optional<AstroImage> dark, flat, bias, darkFlat, badPixels;
};
AstroImage masterFrame(int count, const ImageLoader&, bool normalize, const CheckCancel& = {}, bool robust = false);
AstroImage detectBadPixels(const AstroImage& dark,double sigma=8);
AstroImage calibrate(AstroImage light, const Calibration&, const CheckCancel& = {});
FloatImage linearRgb(const AstroImage&, const CheckCancel& = {});
struct Star { double x{},y{},flux{},fwhm{}; };
std::vector<Star> detectStars(const FloatImage&, const CheckCancel& = {},int maximumStars=150);
struct StarAlignment {
    std::array<double,6> matrix{1,0,0,0,1,0}; // source -> reference
    int matches{};
    double residual{};
    double fluxScale{1};
};
StarAlignment alignStars(const FloatImage& reference,const FloatImage& source,const CheckCancel& = {});
class WcsProjection;
struct WcsRegistrationCheck { int frame{},matches{},sipOrder{};double residual{}; };
struct DrizzleOptions {
    double scale{2}, pixelFraction{0.8};
    bool nativeBayer{}; // Explicit opt-in; existing projects keep RGB drizzle.
};
// Exact overlap for affine drops; optional WCS/SIP drops are adaptively
// subdivided at a 0.001 output-pixel midpoint tolerance. Values retain the
// source intensity convention; no pixel-area photometry calibration is applied.
struct DrizzleProjection { FloatImage image; std::vector<float> weights; int weightChannels{1}; };
std::array<int,2> drizzleDimensions(int width,int height,const DrizzleOptions&);
DrizzleProjection drizzleProject(const FloatImage&,const StarAlignment&,int referenceWidth,int referenceHeight,
                                 const DrizzleOptions&,const CheckCancel& = {},const WcsProjection* = nullptr);
// Calibrated CFA values go only to their measured R/G/B channel. Weights are
// interleaved RGB; zero weight means missing data, including a real zero value.
DrizzleProjection drizzleProjectBayer(const AstroImage&,const StarAlignment&,int referenceWidth,int referenceHeight,
                                      const DrizzleOptions&,const CheckCancel& = {},const WcsProjection* = nullptr);
struct BackgroundOptions {
    int degree{1}, samplesPerLine{20};
    double tolerance{1};
    // Normalized image coordinates; empty means automatic generation.
    std::vector<std::array<double,2>> samples;
};
struct BackgroundResult { FloatImage image, model; std::vector<std::array<double,2>> samples; };
BackgroundResult extractBackground(const FloatImage&,const BackgroundOptions&,const CheckCancel& = {});
FloatImage removeGreen(const FloatImage&,int method=0,float amount=1,bool preserveLightness=true,const CheckCancel& = {});
FloatImage orientImage(const FloatImage&,int quarterTurns,bool mirrorHorizontal,bool mirrorVertical,const CheckCancel& = {});
struct Integration {
    FloatImage image;
    // Optional RGB diagnostics on the same cropped grid, after rejection.
    // Weights are geometrical overlap sums, not inverse noise variances.
    FloatImage weights, contributors, rejectionMap;
    std::vector<int> accepted, rejected;
    std::vector<std::string> reasons;
    std::uint64_t rejectedSamples{};
    double totalExposure{};
    int minimumContributors{};
    double meanContributors{};
    bool subpixelDiversity{};
    bool nativeBayer{},wcsProjection{};
    std::vector<WcsRegistrationCheck> wcsChecks;
    std::optional<recording::CelestialWcs> wcs;
    std::optional<double> centerRa,centerDec,pixelScale;
    std::string date;
};
Integration integrate(int count,const ImageLoader&,const Calibration&,bool calibrated,double sigma,
                      const CheckCancel& = {},const Progress& = {},bool normalize=true,const BackgroundOptions* background=nullptr,
                      const DrizzleOptions* drizzle=nullptr,bool diagnosticMaps=false,bool wcsProjection=false,const IntegrationLimits* limits=nullptr);
struct HdrInput { double exposure{}, saturation{1}; };
struct HdrResult {
    FloatImage image, recoveredMap, clippedMap;
    std::uint64_t recoveredPixels{}, clippedPixels{};
    std::vector<StarAlignment> alignments;
};
// Linear calibrated data, identical sensor units/gain/filter. Output units
// match the longest individual exposure. Shared RGB weights protect color.
HdrResult mergeHdr(const std::vector<HdrInput>&,const ImageLoader&,double transition=0.1,const CheckCancel& = {});
struct Finish {
    bool background{};
    BackgroundOptions backgroundOptions;
    bool stretch{true};
    float black{0},white{1},midtones{0.15f};
    std::array<float,3> color{1,1,1};
    float denoise{};
    bool asinh{};
    bool ghs{};
    double ghsStrength{},ghsLocal{},ghsSymmetry{},ghsShadows{},ghsHighlights{1};
    bool ghsInverse{};
    int ghsMode{};
    float asinhStrength{20},saturation{1};
    int asinhLuminance{}; // 0: maximum (legacy), 1: human weighted, 2: mean
    ComputePreference compute{ComputePreference::Auto};
};
FloatImage finishDeepSky(const FloatImage&,const Finish&,const CheckCancel& = {},ComputeReport* = nullptr,const PreviewTarget* preview = nullptr);
FloatImage combineChannels(const FloatImage& red,const FloatImage& green,const FloatImage& blue,
                          const FloatImage* luminance=nullptr,const CheckCancel& = {});
}
