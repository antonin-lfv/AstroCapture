#include "UvcFormat.hpp"

#include <cstring>

namespace astro::driver::uvc {

using astro::domain::BayerPattern;
using astro::domain::PixelStorage;

namespace {

bool fourccIs(const uvc_format_desc_t& desc, const char (&literal)[5]) {
    return std::memcmp(desc.fourccFormat, literal, 4) == 0;
}

}  // namespace

std::optional<RawFormatMatch> bestRawFormat(const uvc_format_desc_t* formats) {
    // Explicit-pattern Bayer FourCCs first — the device tells us the exact
    // mosaic layout, nothing to guess. "BY8" is generic 8-bit Bayer with no
    // pattern encoded in the FourCC itself; GBRG here follows what
    // oaCapture's own history found correct for this camera family (see
    // ADR-004) — a best guess pending direct verification against a real
    // Bayer test target, not a spec guarantee. Mono is the last resort: it
    // streams, but throws the sensor's color information away.
    const uvc_format_desc_t* by8 = nullptr;
    const uvc_format_desc_t* mono8 = nullptr;
    const uvc_format_desc_t* mono16 = nullptr;

    for (const uvc_format_desc_t* desc = formats; desc != nullptr; desc = desc->next) {
        if (desc->bDescriptorSubtype != UVC_VS_FORMAT_UNCOMPRESSED &&
            desc->bDescriptorSubtype != UVC_VS_FORMAT_FRAME_BASED) {
            continue;
        }
        if (fourccIs(*desc, "GRBG")) {
            return RawFormatMatch{PixelStorage::Bayer8, BayerPattern::GRBG, UVC_FRAME_FORMAT_SGRBG8, desc};
        }
        if (fourccIs(*desc, "GBRG")) {
            return RawFormatMatch{PixelStorage::Bayer8, BayerPattern::GBRG, UVC_FRAME_FORMAT_SGBRG8, desc};
        }
        if (fourccIs(*desc, "RGGB")) {
            return RawFormatMatch{PixelStorage::Bayer8, BayerPattern::RGGB, UVC_FRAME_FORMAT_SRGGB8, desc};
        }
        if (fourccIs(*desc, "BGGR")) {
            return RawFormatMatch{PixelStorage::Bayer8, BayerPattern::BGGR, UVC_FRAME_FORMAT_SBGGR8, desc};
        }
        if (by8 == nullptr && fourccIs(*desc, "BY8 ")) {
            by8 = desc;
        }
        if (mono8 == nullptr && (fourccIs(*desc, "Y800") || fourccIs(*desc, "GREY"))) {
            mono8 = desc;
        }
        if (mono16 == nullptr && fourccIs(*desc, "Y16 ")) {
            mono16 = desc;
        }
    }

    if (by8 != nullptr) {
        return RawFormatMatch{PixelStorage::Bayer8, BayerPattern::GBRG, UVC_FRAME_FORMAT_BY8, by8};
    }
    if (mono8 != nullptr) {
        return RawFormatMatch{PixelStorage::Mono8, BayerPattern::None, UVC_FRAME_FORMAT_GRAY8, mono8};
    }
    if (mono16 != nullptr) {
        return RawFormatMatch{PixelStorage::Mono16, BayerPattern::None, UVC_FRAME_FORMAT_GRAY16, mono16};
    }
    return std::nullopt;
}

}  // namespace astro::driver::uvc
