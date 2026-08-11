#include "astro/recording/FitsWriter.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Check.hpp"

using astro::recording::writeFits8Rgb;

int main() {
    const std::string path = "astro_fits_writer_test.fits";
    constexpr std::uint32_t kWidth = 4;
    constexpr std::uint32_t kHeight = 2;

    std::vector<std::uint8_t> rgb8(static_cast<std::size_t>(kWidth) * kHeight * 3);
    for (std::size_t i = 0; i < rgb8.size(); ++i) {
        rgb8[i] = static_cast<std::uint8_t>((i * 11) % 256);
    }

    ASTRO_TEST_CHECK(writeFits8Rgb(path, kWidth, kHeight, rgb8.data()).ok());

    std::ifstream file(path, std::ios::binary);
    ASTRO_TEST_CHECK(file.is_open());
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // FITS requires the file be a whole number of 2880-byte blocks.
    ASTRO_TEST_CHECK(!bytes.empty());
    ASTRO_TEST_CHECK(bytes.size() % 2880 == 0);

    const std::string header(reinterpret_cast<const char*>(bytes.data()), 2880);
    ASTRO_TEST_CHECK(header.substr(0, 6) == "SIMPLE");
    ASTRO_TEST_CHECK(header.find("BITPIX") != std::string::npos);
    ASTRO_TEST_CHECK(header.find("NAXIS1") != std::string::npos);
    ASTRO_TEST_CHECK(header.find("NAXIS2") != std::string::npos);
    ASTRO_TEST_CHECK(header.find("NAXIS3") != std::string::npos);
    ASTRO_TEST_CHECK(header.find("END") != std::string::npos);
    // Every card must be exactly 80 characters: END must land on a
    // multiple-of-80 boundary, not drift from malformed card padding.
    ASTRO_TEST_CHECK(header.find("END") % 80 == 0);

    // This header is well under one 2880-byte block, so data starts at the
    // next block boundary: offset 2880.
    const std::size_t dataOffset = 2880;
    const std::size_t planeSize = static_cast<std::size_t>(kWidth) * kHeight;
    ASTRO_TEST_CHECK(bytes.size() >= dataOffset + planeSize * 3);
    for (std::size_t i = 0; i < planeSize; ++i) {
        ASTRO_TEST_CHECK(bytes[dataOffset + i] == rgb8[i * 3 + 0]);                  // R plane
        ASTRO_TEST_CHECK(bytes[dataOffset + planeSize + i] == rgb8[i * 3 + 1]);      // G plane
        ASTRO_TEST_CHECK(bytes[dataOffset + planeSize * 2 + i] == rgb8[i * 3 + 2]);  // B plane
    }

    ASTRO_TEST_CHECK(!writeFits8Rgb(path, 0, 0, rgb8.data()).ok());

    std::remove(path.c_str());
    std::cout << "OK\n";
    return 0;
}
