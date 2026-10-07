#pragma once
#include <astro/domain/PixelFormat.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace astro::recording {
// Read-only, seekable SER v3 input. One reader belongs to one worker.
// Throws std::runtime_error for malformed, unsupported or truncated input.
class SerReader {
public:
    explicit SerReader(const std::filesystem::path& path);
    std::vector<std::uint16_t> read(std::uint32_t index);
    std::uint32_t width{}, height{}, count{}, bits{}, channels{};
    domain::BayerPattern bayer{domain::BayerPattern::None};

private:
    std::ifstream file_;
    std::uint64_t frameBytes_{};
    bool bigEndian_{}, bgr_{};
};
}  // namespace astro::recording
