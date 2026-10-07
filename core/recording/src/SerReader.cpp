#include <algorithm>
#include <array>
#include <astro/recording/SerReader.hpp>
#include <stdexcept>
#include <string_view>

namespace astro::recording {
SerReader::SerReader(const std::filesystem::path& path) : file_(path, std::ios::binary) {
    std::array<unsigned char, 178> h{};
    if (!file_.read(reinterpret_cast<char*>(h.data()), h.size()) ||
        std::string_view(reinterpret_cast<char*>(h.data()), 14) != "LUCAM-RECORDER")
        throw std::runtime_error("Not a valid SER file (missing or incomplete header).");
    const auto u32 = [&](int p) {
        return std::uint32_t(h[p]) | (std::uint32_t(h[p + 1]) << 8) | (std::uint32_t(h[p + 2]) << 16) |
               (std::uint32_t(h[p + 3]) << 24);
    };
    const auto color = u32(18), endian = u32(22);
    width = u32(26);
    height = u32(30);
    bits = u32(34);
    count = u32(38);
    // Bound per-frame memory and per-sequence quality metadata before allocating.
    if (!width || !height || std::uint64_t(width) * height > 16000000 || !count || count > 10000000 ||
        !bits || bits > 16 || endian > 1)
        throw std::runtime_error(
            "Unsupported SER dimensions, depth or frame count (maximum 16 MP / 10 million frames).");
    channels = 1;
    using domain::BayerPattern;
    switch (color) {
        case 0:
            break;
        case 8:
            bayer = BayerPattern::RGGB;
            break;
        case 9:
            bayer = BayerPattern::GRBG;
            break;
        case 10:
            bayer = BayerPattern::GBRG;
            break;
        case 11:
            bayer = BayerPattern::BGGR;
            break;
        case 100:
            channels = 3;
            break;
        case 101:
            channels = 3;
            bgr_ = true;
            break;
        default:
            throw std::runtime_error("Unsupported SER color format (mono, RGB and Bayer are supported).");
    }
    if (bayer != BayerPattern::None && (width < 2 || height < 2))
        throw std::runtime_error("A Bayer frame must contain at least one 2 by 2 tile.");
    // De facto convention used by Siril/SER Player, also used by SerRecorder.
    bigEndian_ = endian == 1;
    frameBytes_ = std::uint64_t(width) * height * channels * (bits > 8 ? 2 : 1);
    file_.seekg(0, std::ios::end);
    const auto length = file_.tellg();
    const auto required = 178 + frameBytes_ * count;
    if (length < 0 || std::uint64_t(length) < required)
        throw std::runtime_error("SER video is truncated: declared frames are missing.");
    const auto trailer = std::uint64_t(length) - required;
    if (trailer != 0 && trailer != std::uint64_t(count) * 8)
        throw std::runtime_error("SER has an incomplete timestamp trailer or unexpected trailing data.");
}
std::vector<std::uint16_t> SerReader::read(std::uint32_t index) {
    if (index >= count) throw std::runtime_error("SER frame index is out of range.");
    file_.clear();
    file_.seekg(static_cast<std::streamoff>(178 + frameBytes_ * index));
    std::vector<unsigned char> bytes(static_cast<std::size_t>(frameBytes_));
    if (!file_.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Cannot read SER frame: the source may have changed or become unavailable.");
    std::vector<std::uint16_t> samples(std::size_t(width) * height * channels);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        samples[i] = bits <= 8    ? bytes[i]
                     : bigEndian_ ? std::uint16_t((bytes[2 * i] << 8) | bytes[2 * i + 1])
                                  : std::uint16_t(bytes[2 * i] | (bytes[2 * i + 1] << 8));
    }
    if (bgr_)
        for (std::size_t i = 0; i < samples.size(); i += 3) std::swap(samples[i], samples[i + 2]);
    return samples;
}
}  // namespace astro::recording
