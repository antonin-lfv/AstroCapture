#include "astro/recording/TiffWriter.hpp"

#include <array>
#include <algorithm>
#include <fstream>
#include <vector>
#include <limits>
#include <filesystem>

namespace astro::recording {

using namespace astro::domain;

namespace {

constexpr std::uint16_t kTypeShort = 3;
constexpr std::uint16_t kTypeLong = 4;
constexpr std::uint16_t kTypeRational = 5;

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

// A baseline-TIFF IFD entry: tag, type, count, then either the value itself
// (if it fits in 4 bytes) or an offset to it elsewhere in the file.
void appendIfdEntry(std::vector<std::uint8_t>& out, std::uint16_t tag, std::uint16_t type, std::uint32_t count,
                     std::uint32_t valueOrOffset) {
    appendU16(out, tag);
    appendU16(out, type);
    appendU32(out, count);
    appendU32(out, valueOrOffset);
}

// SHORT/count-1 values are stored left-justified (low bytes first, for
// little-endian "II") in the entry's 4-byte value field, per TIFF 6.0.
std::uint32_t inlineShort(std::uint16_t value) { return static_cast<std::uint32_t>(value); }

}  // namespace

template <typename Sample>
static Result<void> writeTiffImpl(const std::string& path, std::uint32_t width, std::uint32_t height,
                             const Sample* rgb8, std::span<const std::uint8_t> icc = {}) {
    if (width == 0 || height == 0 || rgb8 == nullptr ||
        std::uint64_t(width)*height > (std::numeric_limits<std::uint32_t>::max()-1024)/6) {
        return Result<void>::failure({ErrorCode::InvalidFrame, "writeTiff16Rgb: empty image", path});
    }

    const std::uint16_t kTagCount = icc.empty()?13:14;
    constexpr std::uint32_t kHeaderSize = 8;
    const std::uint32_t kIfdSize = 2 + kTagCount * 12 + 4;
    const std::uint32_t ifdOffset = kHeaderSize;
    const std::uint32_t externalBlobOffset = ifdOffset + kIfdSize;
    const std::uint32_t bitsPerSampleOffset = externalBlobOffset;      // 3x uint16 = 6 bytes
    const std::uint32_t xResolutionOffset = bitsPerSampleOffset + 6;   // 2x uint32 = 8 bytes
    const std::uint32_t yResolutionOffset = xResolutionOffset + 8;     // 2x uint32 = 8 bytes
    const std::uint32_t iccOffset = yResolutionOffset + 8;
    const std::uint32_t pixelDataOffset = iccOffset + std::uint32_t(icc.size()) + (icc.size()%2);

    const std::uint64_t pixelCount = static_cast<std::uint64_t>(width) * height;
    const std::uint32_t pixelDataSize = static_cast<std::uint32_t>(pixelCount * 3 * 2);  // 3 channels, 16-bit each

    std::vector<std::uint8_t> out;
    out.reserve(pixelDataOffset);

    // --- Header ---
    out.push_back('I');
    out.push_back('I');
    appendU16(out, 42);
    appendU32(out, ifdOffset);

    // --- IFD (tags in ascending order, as TIFF 6.0 recommends) ---
    appendU16(out, kTagCount);
    appendIfdEntry(out, 256, kTypeLong, 1, width);                        // ImageWidth
    appendIfdEntry(out, 257, kTypeLong, 1, height);                       // ImageLength
    appendIfdEntry(out, 258, kTypeShort, 3, bitsPerSampleOffset);         // BitsPerSample
    appendIfdEntry(out, 259, kTypeShort, 1, inlineShort(1));              // Compression: none
    appendIfdEntry(out, 262, kTypeShort, 1, inlineShort(2));              // PhotometricInterpretation: RGB
    appendIfdEntry(out, 273, kTypeLong, 1, pixelDataOffset);              // StripOffsets
    appendIfdEntry(out, 277, kTypeShort, 1, inlineShort(3));              // SamplesPerPixel
    appendIfdEntry(out, 278, kTypeLong, 1, height);                       // RowsPerStrip: one strip
    appendIfdEntry(out, 279, kTypeLong, 1, pixelDataSize);                // StripByteCounts
    appendIfdEntry(out, 282, kTypeRational, 1, xResolutionOffset);        // XResolution
    appendIfdEntry(out, 283, kTypeRational, 1, yResolutionOffset);        // YResolution
    appendIfdEntry(out, 284, kTypeShort, 1, inlineShort(1));              // PlanarConfiguration: chunky
    appendIfdEntry(out, 296, kTypeShort, 1, inlineShort(2));              // ResolutionUnit: inches
    if(!icc.empty())appendIfdEntry(out,34675,7,std::uint32_t(icc.size()),iccOffset);
    appendU32(out, 0);                                                    // next IFD offset: none

    // --- External values the inline 4-byte slots couldn't hold ---
    appendU16(out, 16);
    appendU16(out, 16);
    appendU16(out, 16);              // BitsPerSample x3
    appendU32(out, 72);
    appendU32(out, 1);               // XResolution 72/1
    appendU32(out, 72);
    appendU32(out, 1);               // YResolution 72/1

    out.insert(out.end(),icc.begin(),icc.end());if(icc.size()%2)out.push_back(0);

    std::ofstream file(std::filesystem::path(std::u8string(path.begin(), path.end())), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return Result<void>::failure({ErrorCode::RecordingOpenFailed, "Failed to open file for writing", path});
    }
    file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
    // The baseline TIFF layout stays identical, but only a 128 KiB pixel block
    // is encoded at once. Large exports no longer need a second complete file.
    std::array<std::uint8_t,131072> block{};
    for(std::uint64_t at=0;at<pixelCount*3;) {
        const auto count=std::min<std::uint64_t>(block.size()/2,pixelCount*3-at);
        for(std::uint64_t i=0;i<count;++i) {
            const std::uint16_t value=sizeof(Sample)==1?static_cast<std::uint16_t>(rgb8[at+i]*257u):rgb8[at+i];
            block[i*2]=std::uint8_t(value&255);block[i*2+1]=std::uint8_t(value>>8);
        }
        file.write(reinterpret_cast<const char*>(block.data()),std::streamsize(count*2));
        if(!file.good())return Result<void>::failure({ErrorCode::DiskWriteFailed,"Failed writing TIFF data",path});
        at+=count;
    }
    if (!file.good()) {
        return Result<void>::failure({ErrorCode::DiskWriteFailed, "Failed writing TIFF data", path});
    }
    return Result<void>::success();
}

Result<void> writeTiff16Rgb(const std::string& path, std::uint32_t width, std::uint32_t height, const std::uint8_t* rgb) {
    return writeTiffImpl(path, width, height, rgb);
}
Result<void> writeTiff16Samples(const std::string& path, std::uint32_t width, std::uint32_t height, const std::uint16_t* rgb, std::span<const std::uint8_t> icc) {
    return writeTiffImpl(path, width, height, rgb,icc);
}
}  // namespace astro::recording
