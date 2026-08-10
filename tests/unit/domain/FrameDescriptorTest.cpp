#include "astro/domain/Frame.hpp"

#include "Check.hpp"

using astro::domain::BayerPattern;
using astro::domain::FrameDescriptor;
using astro::domain::PixelStorage;

int main() {
    ASTRO_TEST_CHECK(astro::domain::bytesPerPixel(PixelStorage::Unknown) == 0);
    ASTRO_TEST_CHECK(astro::domain::bytesPerPixel(PixelStorage::Mono8) == 1);
    ASTRO_TEST_CHECK(astro::domain::bytesPerPixel(PixelStorage::Bayer8) == 1);
    ASTRO_TEST_CHECK(astro::domain::bytesPerPixel(PixelStorage::Mono16) == 2);
    ASTRO_TEST_CHECK(astro::domain::bytesPerPixel(PixelStorage::Bayer16) == 2);
    ASTRO_TEST_CHECK(astro::domain::bytesPerPixel(PixelStorage::RGB24) == 3);
    ASTRO_TEST_CHECK(astro::domain::bytesPerPixel(PixelStorage::BGR24) == 3);

    // No explicit stride: derived from width * bytesPerPixel(storage).
    FrameDescriptor withoutStride;
    withoutStride.width = 640;
    withoutStride.height = 480;
    withoutStride.storage = PixelStorage::Bayer8;
    withoutStride.bayer = BayerPattern::RGGB;
    ASTRO_TEST_CHECK(withoutStride.expectedPayloadBytes() == 640u * 480u);

    // Explicit stride (row padding) must win over the derived value.
    FrameDescriptor withStride = withoutStride;
    withStride.strideBytes = 700;
    ASTRO_TEST_CHECK(withStride.expectedPayloadBytes() == 700u * 480u);

    FrameDescriptor mono16;
    mono16.width = 320;
    mono16.height = 240;
    mono16.storage = PixelStorage::Mono16;
    ASTRO_TEST_CHECK(mono16.expectedPayloadBytes() == 320u * 240u * 2u);

    std::cout << "OK\n";
    return 0;
}
