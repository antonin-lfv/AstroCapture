#include "SimpleDebayer.hpp"

#include <cstdint>

namespace astro::app {

using astro::domain::BayerPattern;
using astro::domain::FrameDescriptor;
using astro::domain::PixelStorage;

namespace {

// Reads the 2x2 tile at (x, y) (both even) and returns its flat RGB color,
// averaging the two green samples. `pattern` gives the channel layout of the
// tile's top-left/top-right/bottom-left/bottom-right samples.
QRgb sampleTileColor(const std::uint8_t* bytes, std::uint32_t stride, std::uint32_t x, std::uint32_t y,
                      BayerPattern pattern) {
    const std::uint8_t p00 = bytes[static_cast<std::size_t>(y) * stride + x];
    const std::uint8_t p01 = bytes[static_cast<std::size_t>(y) * stride + x + 1];
    const std::uint8_t p10 = bytes[static_cast<std::size_t>(y + 1) * stride + x];
    const std::uint8_t p11 = bytes[static_cast<std::size_t>(y + 1) * stride + x + 1];

    std::uint8_t r = 0;
    std::uint8_t g1 = 0;
    std::uint8_t g2 = 0;
    std::uint8_t b = 0;
    switch (pattern) {
        case BayerPattern::RGGB:
            r = p00; g1 = p01; g2 = p10; b = p11;
            break;
        case BayerPattern::BGGR:
            b = p00; g1 = p01; g2 = p10; r = p11;
            break;
        case BayerPattern::GRBG:
            g1 = p00; r = p01; b = p10; g2 = p11;
            break;
        case BayerPattern::GBRG:
            g1 = p00; b = p01; r = p10; g2 = p11;
            break;
        case BayerPattern::None:
            break;
    }
    const std::uint8_t g = static_cast<std::uint8_t>((static_cast<int>(g1) + g2) / 2);
    return qRgb(r, g, b);
}

}  // namespace

QImage debayerToPreviewImage(const FrameDescriptor& descriptor, const std::byte* data, std::size_t size) {
    QImage image(static_cast<int>(descriptor.width), static_cast<int>(descriptor.height), QImage::Format_RGB32);

    const std::uint32_t stride =
        descriptor.strideBytes != 0 ? descriptor.strideBytes : descriptor.width;
    const bool supported = descriptor.storage == PixelStorage::Bayer8 && descriptor.bayer != BayerPattern::None &&
                            static_cast<std::size_t>(stride) * descriptor.height <= size;
    if (!supported) {
        image.fill(Qt::black);
        return image;
    }

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
    for (std::uint32_t y = 0; y + 1 < descriptor.height; y += 2) {
        for (std::uint32_t x = 0; x + 1 < descriptor.width; x += 2) {
            const QRgb color = sampleTileColor(bytes, stride, x, y, descriptor.bayer);
            image.setPixel(static_cast<int>(x), static_cast<int>(y), color);
            image.setPixel(static_cast<int>(x + 1), static_cast<int>(y), color);
            image.setPixel(static_cast<int>(x), static_cast<int>(y + 1), color);
            image.setPixel(static_cast<int>(x + 1), static_cast<int>(y + 1), color);
        }
    }
    return image;
}

}  // namespace astro::app
