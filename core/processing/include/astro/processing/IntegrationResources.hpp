#pragma once
#include <cstdint>
#include <filesystem>

namespace astro::processing {
struct IntegrationLimits {
    // Bounds the combined float strip buffers, not the whole process.
    std::uint64_t stripBytes{64ULL * 1024 * 1024};
    std::uint64_t diskReserveBytes{256ULL * 1024 * 1024};
    std::filesystem::path temporaryDirectory;
};
struct IntegrationResourceRequest {
    int width{}, height{}, channels{3}, count{};
    bool drizzle{}, nativeBayer{}, diagnosticMaps{}, sequenceBackground{}, wcsProjection{};
    double scale{1};
    std::uint64_t calibrationBytes{};
};
struct IntegrationResourceEstimate {
    int width{}, height{}, stripRows{}, stripColumns{};
    std::uint64_t temporaryBytes{}, stripBytes{}, resultBytes{}, workingBytes{};
};
// Full output grid before common-coverage cropping, all selected exposures.
// Working memory is a conservative model of native buffers, not an RSS promise.
IntegrationResourceEstimate estimateIntegrationResources(const IntegrationResourceRequest&, const IntegrationLimits& = {});
int integrationStripRows(int width, int count, int weightChannels, std::uint64_t budget);
int integrationStripColumns(int width, int count, int weightChannels, std::uint64_t budget);
std::uint64_t physicalMemoryBytes(); // zero if unavailable
}
