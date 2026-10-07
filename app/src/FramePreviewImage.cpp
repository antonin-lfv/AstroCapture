#include "FramePreviewImage.hpp"

#include <cstdint>
#include <cmath>
#include <cstring>
#include <algorithm>

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
        default: {
            const auto stride = descriptor.strideBytes ? descriptor.strideBytes : descriptor.width * astro::domain::bytesPerPixel(descriptor.storage);
            if(!data || !stride || size < std::size_t(stride)*descriptor.height || !descriptor.validBits || descriptor.validBits>16) return blackImage(descriptor);
            const auto* bytes=reinterpret_cast<const unsigned char*>(data);
            const bool wide=descriptor.storage==PixelStorage::Mono16||descriptor.storage==PixelStorage::Bayer16;
            const bool mono=descriptor.storage==PixelStorage::Mono8||wide;
            const bool rgb=descriptor.storage==PixelStorage::RGB24||descriptor.storage==PixelStorage::BGR24;
            if(!mono&&!rgb)return blackImage(descriptor);
            QImage image(int(descriptor.width),int(descriptor.height),QImage::Format_RGB888);
            const double scale=255.0 / double((1u<<descriptor.validBits)-1);
            auto sample=[&](int x,int y){const auto* p=bytes+std::size_t(y)*stride+x*(wide?2:1);if(wide){std::uint16_t v;std::memcpy(&v,p,2);return double(v);}return double(*p);};
            for(int y=0;y<int(descriptor.height);++y) {
                auto* row=image.scanLine(y);
                for(int x=0;x<int(descriptor.width);++x)for(int c=0;c<3;++c) {
                    double value=0;
                    if(rgb)value=bytes[std::size_t(y)*stride+x*3+(descriptor.storage==PixelStorage::BGR24?2-c:c)];
                    else if(descriptor.bayer==BayerPattern::None||astro::domain::bayerChannelAt(descriptor.bayer,x,y)==c)value=sample(x,y)*scale;
                    else {int n=0;for(int yy=std::max(0,y-1);yy<=std::min(int(descriptor.height)-1,y+1);++yy)for(int xx=std::max(0,x-1);xx<=std::min(int(descriptor.width)-1,x+1);++xx)if(astro::domain::bayerChannelAt(descriptor.bayer,xx,yy)==c){value+=sample(xx,yy);++n;}value=n?value*scale/n:sample(x,y)*scale;}
                    row[x*3+c]=uchar(std::lround(std::clamp(value,0.,255.)));
                }
            }
            return image;
        }
    }
}

}  // namespace astro::app
