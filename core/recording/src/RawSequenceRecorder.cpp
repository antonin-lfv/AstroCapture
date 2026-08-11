#include "astro/recording/RawSequenceRecorder.hpp"

#include <array>
#include <chrono>

namespace astro::recording {

using namespace astro::domain;

namespace {

// On-disk layout (little-endian on every platform this project targets):
//   char     magic[8]        = "ACRAWSEQ"
//   uint32   formatVersion   = 2
//   uint32   width
//   uint32   height
//   uint32   strideBytes
//   uint32   pixelStorage    (astro::domain::PixelStorage)
//   uint32   bayerPattern    (astro::domain::BayerPattern)
//   uint32   validBits
//   uint64   frameCount      (0 until close() patches it)
//   double   nominalFps
//   int64    anchorSteadyNanos      (steady_clock::now().time_since_epoch()
//                                     at open(), same clock/epoch as each
//                                     frame's hostArrivalNanos below)
//   int64    anchorSystemUnixNanos  (system_clock::now().time_since_epoch()
//                                     at open(), i.e. Unix epoch time)
// followed by `frameCount` records of:
//   uint64   sequence
//   int64    hostArrivalNanos  (astro::domain::FrameMetadata::hostArrival's
//                                steady_clock duration since its own epoch;
//                                meaningless as an absolute time on its
//                                own — RawSequenceReader's
//                                frameWallClockUnixNanos() combines it with
//                                the two anchors above to recover the
//                                frame's actual Unix time, which per-frame
//                                derotation tools such as WinJUPOS need)
//   byte     payload[descriptor.expectedPayloadBytes()]
constexpr std::array<char, 8> kMagic = {'A', 'C', 'R', 'A', 'W', 'S', 'E', 'Q'};
constexpr std::uint32_t kFormatVersion = 2;

template <typename T>
void writeField(std::ofstream& file, const T& value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

}  // namespace

RawSequenceRecorder::~RawSequenceRecorder() {
    if (file_.is_open()) {
        close();
    }
}

Result<void> RawSequenceRecorder::open(const std::string& path, const FrameDescriptor& descriptor,
                                        double nominalFps) {
    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        return Result<void>::failure({ErrorCode::RecordingOpenFailed, "Failed to open file for writing", path});
    }

    descriptor_ = descriptor;
    framesWritten_ = 0;

    file_.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
    writeField(file_, kFormatVersion);
    writeField(file_, descriptor.width);
    writeField(file_, descriptor.height);
    writeField(file_, descriptor.strideBytes);
    writeField(file_, static_cast<std::uint32_t>(descriptor.storage));
    writeField(file_, static_cast<std::uint32_t>(descriptor.bayer));
    writeField(file_, static_cast<std::uint32_t>(descriptor.validBits));

    frameCountFieldOffset_ = file_.tellp();
    writeField(file_, std::uint64_t{0});
    writeField(file_, nominalFps);

    // Captured back-to-back so the two clocks stay in sync to within a
    // few microseconds — good enough for reconstructing per-frame wall
    // time from the steady_clock deltas frames are stamped with.
    const auto anchorSteadyNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    const auto anchorSystemUnixNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();
    writeField(file_, anchorSteadyNanos);
    writeField(file_, anchorSystemUnixNanos);

    if (!file_.good()) {
        return Result<void>::failure({ErrorCode::RecordingOpenFailed, "Failed writing header", path});
    }
    return Result<void>::success();
}

Result<void> RawSequenceRecorder::writeFrame(const FrameBuffer& frame) {
    if (!file_.is_open()) {
        return Result<void>::failure({ErrorCode::InternalInvariantViolation, "writeFrame() before open()", ""});
    }
    if (!frame.storage) {
        return Result<void>::failure({ErrorCode::InvalidFrame, "Frame has no backing storage", ""});
    }
    if (frame.storage->size() != descriptor_.expectedPayloadBytes()) {
        return Result<void>::failure(
            {ErrorCode::InvalidFrame, "Frame payload size does not match the recording's descriptor", ""});
    }

    writeField(file_, frame.metadata.sequence);
    const auto hostArrivalNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(frame.metadata.hostArrival.time_since_epoch()).count();
    writeField(file_, hostArrivalNanos);
    file_.write(reinterpret_cast<const char*>(frame.storage->data()),
                static_cast<std::streamsize>(frame.storage->size()));

    if (!file_.good()) {
        return Result<void>::failure({ErrorCode::DiskWriteFailed, "Failed writing frame", ""});
    }

    ++framesWritten_;
    return Result<void>::success();
}

Result<RecordingSummary> RawSequenceRecorder::close() {
    if (!file_.is_open()) {
        return Result<RecordingSummary>::success(RecordingSummary{framesWritten_});
    }

    file_.seekp(frameCountFieldOffset_);
    writeField(file_, framesWritten_);
    const bool wroteFinalCount = file_.good();
    file_.close();

    if (!wroteFinalCount || file_.fail()) {
        return Result<RecordingSummary>::failure({ErrorCode::DiskWriteFailed, "Failed finalizing recording", ""});
    }
    return Result<RecordingSummary>::success(RecordingSummary{framesWritten_});
}

}  // namespace astro::recording
