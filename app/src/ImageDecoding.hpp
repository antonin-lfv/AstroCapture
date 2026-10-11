#pragma once
#include <QImageReader>
#include <mutex>
namespace astro::app {
inline void ensureLargeImageDecoding() {
    static std::once_flag once;
    // 128 MP in RGBA64 fits below 1 GiB. Pixel/dimension and working-set
    // checks remain mandatory before calling the decoder.
    std::call_once(once,[]{QImageReader::setAllocationLimit(1024);});
}
}
