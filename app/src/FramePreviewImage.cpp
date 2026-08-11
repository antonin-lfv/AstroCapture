#include "FramePreviewImage.hpp"

#include <cstdint>

#include "astro/processing/Debayer.hpp"

namespace astro::app {

using astro::domain::BayerPattern;
using astro::domain::FrameDescriptor;
using astro::domain::PixelStorage;

namespace {

QImage blackImage(const FrameDescriptor& descriptor) {
    QImage image(static_cast<int>(descriptor.width), static_cast<int>(descriptor.height), QImage::Format_RGB32);
    image.fill(Qt::black);
    return image;
}

QImage demosaicBayer8(const FrameDescriptor& descriptor, const std::byte* data, std::size_t size) {
    const auto rgb = astro::processing::debayerBilinear(descriptor, data, size);
    if (rgb.empty()) {
        return blackImage(descriptor);
    }

    const QImage view(rgb.data(), static_cast<int>(descriptor.width), static_cast<int>(descriptor.height),
                       static_cast<int>(descriptor.width) * 3, QImage::Format_RGB888);
    // `rgb` is a local about to go out of scope, so the returned image must
    // own a private copy rather than viewing it.
    return view.copy();
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
