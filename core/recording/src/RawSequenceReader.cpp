#include "astro/recording/RawSequenceReader.hpp"

#include <array>
#include <chrono>
#include <string_view>
#include <vector>

namespace astro::recording {

using namespace astro::domain;

namespace {

constexpr std::array<char, 8> kMagic = {'A', 'C', 'R', 'A', 'W', 'S', 'E', 'Q'};

template <typename T>
void readField(std::ifstream& file, T& value) {
    file.read(reinterpret_cast<char*>(&value), sizeof(T));
}

}  // namespace

Result<RawSequenceReader::Header> RawSequenceReader::open(const std::string& path) {
    file_.open(path, std::ios::binary);
    if (!file_.is_open()) {
        return Result<Header>::failure({ErrorCode::RecordingOpenFailed, "Failed to open file for reading", path});
    }

    std::array<char, kMagic.size()> magic{};
    file_.read(magic.data(), static_cast<std::streamsize>(magic.size()));
    const std::string_view expected(kMagic.data(), kMagic.size());
    const std::string_view actual(magic.data(), magic.size());
    if (!file_.good() || actual != expected) {
        return Result<Header>::failure({ErrorCode::InvalidFrame, "Not an ACRAWSEQ file", path});
    }

    Header header;
    readField(file_, header.formatVersion);
    readField(file_, header.descriptor.width);
    readField(file_, header.descriptor.height);
    readField(file_, header.descriptor.strideBytes);

    std::uint32_t storageValue = 0;
    std::uint32_t bayerValue = 0;
    std::uint32_t validBitsValue = 0;
    readField(file_, storageValue);
    readField(file_, bayerValue);
    readField(file_, validBitsValue);
    header.descriptor.storage = static_cast<PixelStorage>(storageValue);
    header.descriptor.bayer = static_cast<BayerPattern>(bayerValue);
    header.descriptor.validBits = static_cast<std::uint8_t>(validBitsValue);

    readField(file_, header.frameCount);
    readField(file_, header.nominalFps);
    readField(file_, header.anchorSteadyNanos);
    readField(file_, header.anchorSystemUnixNanos);

    if (!file_.good()) {
        return Result<Header>::failure({ErrorCode::InvalidFrame, "Truncated header", path});
    }

    descriptor_ = header.descriptor;
    frameCount_ = header.frameCount;
    framesRead_ = 0;
    return Result<Header>::success(header);
}

Result<FrameBuffer> RawSequenceReader::readNextFrame() {
    if (framesRead_ >= frameCount_) {
        return Result<FrameBuffer>::failure({ErrorCode::InvalidFrame, "No more frames", ""});
    }

    FrameBuffer frame;
    frame.descriptor = descriptor_;

    readField(file_, frame.metadata.sequence);
    std::int64_t hostArrivalNanos = 0;
    readField(file_, hostArrivalNanos);
    frame.metadata.hostArrival = std::chrono::steady_clock::time_point(std::chrono::nanoseconds(hostArrivalNanos));

    const auto payloadSize = descriptor_.expectedPayloadBytes();
    std::vector<std::byte> bytes(payloadSize);
    file_.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(payloadSize));
    if (!file_.good()) {
        return Result<FrameBuffer>::failure({ErrorCode::InvalidFrame, "Truncated frame data", ""});
    }
    frame.storage = std::make_shared<const BufferStorage>(std::move(bytes));

    ++framesRead_;
    return Result<FrameBuffer>::success(std::move(frame));
}

std::int64_t frameWallClockUnixNanos(const RawSequenceReader::Header& header,
                                      std::chrono::steady_clock::time_point frameHostArrival) {
    const auto frameSteadyNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(frameHostArrival.time_since_epoch()).count();
    return header.anchorSystemUnixNanos + (frameSteadyNanos - header.anchorSteadyNanos);
}

}  // namespace astro::recording
