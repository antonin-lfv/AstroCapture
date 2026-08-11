#include "astro/recording/FitsWriter.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace astro::recording {

using namespace astro::domain;

namespace {

constexpr std::size_t kCardSize = 80;
constexpr std::size_t kBlockSize = 2880;

// Left-justifies `value` into a fixed-width FITS header card
// ("KEYWORD = value"), space-padded to exactly 80 characters — FITS
// requires every card be precisely that width, no delimiter between them.
void appendCard(std::string& header, const std::string& keyword, const std::string& value) {
    std::ostringstream card;
    card.width(8);
    card.setf(std::ios::left);
    card << keyword;
    card << "= ";
    card << value;
    std::string text = card.str();
    text.resize(kCardSize, ' ');
    header += text;
}

void appendEndCard(std::string& header) {
    std::string text = "END";
    text.resize(kCardSize, ' ');
    header += text;
}

}  // namespace

Result<void> writeFits8Rgb(const std::string& path, std::uint32_t width, std::uint32_t height,
                            const std::uint8_t* rgb8) {
    if (width == 0 || height == 0 || rgb8 == nullptr) {
        return Result<void>::failure({ErrorCode::InvalidFrame, "writeFits8Rgb: empty image", path});
    }

    std::string header;
    appendCard(header, "SIMPLE", "T");
    appendCard(header, "BITPIX", "8");
    appendCard(header, "NAXIS", "3");
    appendCard(header, "NAXIS1", std::to_string(width));
    appendCard(header, "NAXIS2", std::to_string(height));
    appendCard(header, "NAXIS3", "3");  // R, G, B planes, band-sequential
    appendEndCard(header);
    // FITS headers (and data) are padded to whole 2880-byte blocks.
    header.resize(((header.size() + kBlockSize - 1) / kBlockSize) * kBlockSize, ' ');

    const std::size_t planeSize = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> data(planeSize * 3);
    for (std::size_t i = 0; i < planeSize; ++i) {
        data[i] = rgb8[i * 3 + 0];                // R plane
        data[planeSize + i] = rgb8[i * 3 + 1];     // G plane
        data[planeSize * 2 + i] = rgb8[i * 3 + 2]; // B plane
    }
    const std::size_t paddedDataSize = ((data.size() + kBlockSize - 1) / kBlockSize) * kBlockSize;
    data.resize(paddedDataSize, 0);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return Result<void>::failure({ErrorCode::RecordingOpenFailed, "Failed to open file for writing", path});
    }
    file.write(header.data(), static_cast<std::streamsize>(header.size()));
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!file.good()) {
        return Result<void>::failure({ErrorCode::DiskWriteFailed, "Failed writing FITS data", path});
    }
    return Result<void>::success();
}

}  // namespace astro::recording
