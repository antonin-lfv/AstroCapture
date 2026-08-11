#include "astro/recording/TiffWriter.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

#include "Check.hpp"

using astro::recording::writeTiff16Rgb;

namespace {

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) | (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

}  // namespace

int main() {
    const std::string path = "astro_tiff_writer_test.tiff";
    constexpr std::uint32_t kWidth = 4;
    constexpr std::uint32_t kHeight = 2;

    std::vector<std::uint8_t> rgb8(static_cast<std::size_t>(kWidth) * kHeight * 3);
    for (std::size_t i = 0; i < rgb8.size(); ++i) {
        rgb8[i] = static_cast<std::uint8_t>((i * 7) % 256);
    }

    ASTRO_TEST_CHECK(writeTiff16Rgb(path, kWidth, kHeight, rgb8.data()).ok());

    std::ifstream file(path, std::ios::binary);
    ASTRO_TEST_CHECK(file.is_open());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Header: "II" (little-endian), TIFF magic 42, IFD at offset 8.
    ASTRO_TEST_CHECK(bytes[0] == 'I' && bytes[1] == 'I');
    ASTRO_TEST_CHECK(readU16(bytes, 2) == 42);
    const auto ifdOffset = readU32(bytes, 4);
    ASTRO_TEST_CHECK(ifdOffset == 8);

    const auto tagCount = readU16(bytes, ifdOffset);
    ASTRO_TEST_CHECK(tagCount == 13);

    // ImageWidth (256) must be the first entry, LONG/count1/value==width —
    // ascending tag order is what TIFF 6.0 recommends.
    const std::size_t firstEntry = ifdOffset + 2;
    ASTRO_TEST_CHECK(readU16(bytes, firstEntry) == 256);
    ASTRO_TEST_CHECK(readU16(bytes, firstEntry + 2) == 4);  // type LONG
    ASTRO_TEST_CHECK(readU32(bytes, firstEntry + 4) == 1);
    ASTRO_TEST_CHECK(readU32(bytes, firstEntry + 8) == kWidth);

    // ImageLength (257) immediately follows.
    const std::size_t secondEntry = firstEntry + 12;
    ASTRO_TEST_CHECK(readU16(bytes, secondEntry) == 257);
    ASTRO_TEST_CHECK(readU32(bytes, secondEntry + 8) == kHeight);

    // Scan the IFD for the tags whose values are needed below, rather than
    // hardcoding their entry position, so this test doesn't silently check
    // the wrong bytes if unrelated tags are reordered.
    std::uint32_t stripOffsets = 0;
    std::uint32_t stripByteCounts = 0;
    std::uint32_t bitsPerSampleOffset = 0;
    for (std::uint16_t i = 0; i < tagCount; ++i) {
        const std::size_t entry = ifdOffset + 2 + static_cast<std::size_t>(i) * 12;
        const auto tag = readU16(bytes, entry);
        const auto value = readU32(bytes, entry + 8);
        if (tag == 273) stripOffsets = value;
        if (tag == 279) stripByteCounts = value;
        if (tag == 258) bitsPerSampleOffset = value;
    }
    ASTRO_TEST_CHECK(stripOffsets != 0);
    ASTRO_TEST_CHECK(stripByteCounts == kWidth * kHeight * 3 * 2);
    ASTRO_TEST_CHECK(bitsPerSampleOffset != 0);

    ASTRO_TEST_CHECK(readU16(bytes, bitsPerSampleOffset) == 16);
    ASTRO_TEST_CHECK(readU16(bytes, bitsPerSampleOffset + 2) == 16);
    ASTRO_TEST_CHECK(readU16(bytes, bitsPerSampleOffset + 4) == 16);

    // Every 16-bit sample must be its 8-bit source bit-replicated
    // (v | v << 8): an exact upscale, so 0 stays 0 and 255 stays 65535.
    for (std::size_t i = 0; i < rgb8.size(); ++i) {
        const std::uint16_t expected = static_cast<std::uint16_t>(rgb8[i] | (static_cast<std::uint16_t>(rgb8[i]) << 8));
        const std::uint16_t actual = readU16(bytes, stripOffsets + i * 2);
        ASTRO_TEST_CHECK(actual == expected);
    }

    ASTRO_TEST_CHECK(!writeTiff16Rgb(path, 0, 0, rgb8.data()).ok());

    std::remove(path.c_str());
    std::cout << "OK\n";
    return 0;
}
