#include "FramePreviewImage.hpp"

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

QImage blackImage(const FrameDescriptor& descriptor) {
    QImage image(static_cast<int>(descriptor.width), static_cast<int>(descriptor.height), QImage::Format_RGB32);
    image.fill(Qt::black);
    return image;
}

QImage demosaicBayer8(const FrameDescriptor& descriptor, const std::byte* data, std::size_t size) {
    const std::uint32_t stride = descriptor.strideBytes != 0 ? descriptor.strideBytes : descriptor.width;
    const bool supported = descriptor.bayer != BayerPattern::None &&
                            static_cast<std::size_t>(stride) * descriptor.height <= size;
    if (!supported) {
        return blackImage(descriptor);
    }

    QImage image(static_cast<int>(descriptor.width), static_cast<int>(descriptor.height), QImage::Format_RGB32);
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

QImage wrapBgra32(const FrameDescriptor& descriptor, const std::byte* data, std::size_t size) {
    const std::uint32_t stride = descriptor.strideBytes != 0 ? descriptor.strideBytes : descriptor.width * 4;
    if (static_cast<std::size_t>(stride) * descriptor.height > size) {
        return blackImage(descriptor);
    }

    const auto* bytes = reinterpret_cast<const uchar*>(data);
    QImage view(bytes, static_cast<int>(descriptor.width), static_cast<int>(descriptor.height),
                static_cast<int>(stride), QImage::Format_ARGB32);
    // `data` does not outlive this call (it points into the caller's
    // frame), so the returned image must own a private copy of the bytes.
    return view.copy();
}

}  // namespace

QImage frameToPreviewImage(const FrameDescriptor& descriptor, const std::byte* data, std::size_t size) {
    switch (descriptor.storage) {
        case PixelStorage::Bayer8:
            return demosaicBayer8(descriptor, data, size);
        case PixelStorage::BGRA32:
            return wrapBgra32(descriptor, data, size);
        default:
            return blackImage(descriptor);
    }
}

}  // namespace astro::app
