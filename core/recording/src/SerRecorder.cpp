#include "astro/recording/SerRecorder.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <optional>
#include <limits>

namespace astro::recording {

using namespace astro::domain;

namespace {

// SER header layout (little-endian, 178 bytes total — unchanged since the
// format's introduction):
//   char[14] FileID              = "LUCAM-RECORDER"
//   int32    LuID                = 0 (no real camera series id to report)
//   int32    ColorID             (see serColorIdFor)
//   int32    LittleEndian        = 0 (de facto little-endian convention,
//                                      used by Siril and SER Player)
//   int32    ImageWidth
//   int32    ImageHeight
//   int32    PixelDepthPerPlane  (8 or 16)
//   int32    FrameCount          (0 until close() patches it)
//   char[40] Observer            (unset: left zero-filled)
//   char[40] Instrument
//   char[40] Telescope
//   int64    DateTime            (.NET ticks; UTC used for both fields —
//                                  this project has no separate concept of
//                                  "local time" to report)
//   int64    DateTime_UTC
// followed by FrameCount frames of raw payload bytes (no per-frame
// sequence/timestamp inline, unlike ACRAWSEQ — SER carries timestamps only
// in the trailer below), then an optional trailer of FrameCount int64
// values: each frame's capture time as .NET ticks (100ns intervals since
// 0001-01-01), UTC. Always written here since every frame's hostArrival is
// always available.
constexpr std::array<char, 14> kFileId = {'L', 'U', 'C', 'A', 'M', '-', 'R', 'E', 'C', 'O', 'R', 'D', 'E', 'R'};
constexpr std::size_t kTextFieldSize = 40;

// Ticks (100ns units) between the .NET DateTime epoch (0001-01-01) and the
// Unix epoch (1970-01-01) — the standard, widely-published constant for
// this conversion.
constexpr std::int64_t kUnixEpochTicks = 621355968000000000LL;

std::int64_t unixNanosToDotNetUtcTicks(std::int64_t unixNanos) { return kUnixEpochTicks + unixNanos / 100; }

struct SerFormat {
    std::int32_t colorId;
    std::int32_t planes;
};

// SER's documented ColorID values. Bayer sources stay un-demosaiced
// (single plane, the pattern encoded in the ID) — SER readers debayer on
// load. Returns nullopt for formats SER has no color ID for (BGRA32:
// SER has no alpha channel concept; Unknown).
std::optional<SerFormat> serColorIdFor(const FrameDescriptor& descriptor) {
    switch (descriptor.storage) {
        case PixelStorage::Mono8:
        case PixelStorage::Mono16:
            return SerFormat{0, 1};
        case PixelStorage::Bayer8:
        case PixelStorage::Bayer16:
            switch (descriptor.bayer) {
                case BayerPattern::RGGB:
                    return SerFormat{8, 1};
                case BayerPattern::GRBG:
                    return SerFormat{9, 1};
                case BayerPattern::GBRG:
                    return SerFormat{10, 1};
                case BayerPattern::BGGR:
                    return SerFormat{11, 1};
                case BayerPattern::None:
                    return std::nullopt;
            }
            return std::nullopt;
        case PixelStorage::RGB24:
            return SerFormat{100, 3};
        case PixelStorage::BGR24:
            return SerFormat{101, 3};
        default:
            return std::nullopt;
    }
}

template <typename T>
void writeField(std::ofstream& file, const T& value) {
    file.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void writeTextField(std::ofstream& file, std::size_t size) {
    const std::vector<char> zeros(size, '\0');
    file.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
}

}  // namespace

SerRecorder::~SerRecorder() {
    if (file_.is_open()) {
        close();
    }
}

Result<void> SerRecorder::open(const std::string& path, const FrameDescriptor& descriptor, double /*nominalFps*/) {
    if (file_.is_open()) {
        return Result<void>::failure({ErrorCode::RecordingOpenFailed, "Recorder is already open", path});
    }
    const auto format = serColorIdFor(descriptor);
    if (!format) {
        return Result<void>::failure(
            {ErrorCode::UnsupportedFormat, "SER has no color ID for this frame format", path});
    }

    const auto rowBytes = static_cast<std::size_t>(descriptor.width) * bytesPerPixel(descriptor.storage);
    const bool wide = descriptor.storage == PixelStorage::Mono16 || descriptor.storage == PixelStorage::Bayer16;
    const auto bits = descriptor.validBits != 0 ? descriptor.validBits : (wide ? 16 : 8);
    if (descriptor.width == 0 || descriptor.height == 0 ||
        descriptor.width > std::numeric_limits<std::int32_t>::max() ||
        descriptor.height > std::numeric_limits<std::int32_t>::max() ||
        rowBytes > std::numeric_limits<std::uint32_t>::max() ||
        (descriptor.strideBytes != 0 && descriptor.strideBytes < rowBytes) ||
        (wide ? (bits <= 8 || bits > 16) : bits > 8)) {
        return Result<void>::failure({ErrorCode::InvalidFrame, "Invalid SER dimensions, stride or bit depth", path});
    }

    file_.open(path, std::ios::binary | std::ios::trunc);
    if (!file_.is_open()) {
        return Result<void>::failure({ErrorCode::RecordingOpenFailed, "Failed to open file for writing", path});
    }

    descriptor_ = descriptor;
    framesWritten_ = 0;
    frameTimestampsUtcTicks_.clear();

    anchorSteadyNanos_ =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    anchorSystemUnixNanos_ =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
            .count();

    file_.write(kFileId.data(), static_cast<std::streamsize>(kFileId.size()));
    writeField(file_, std::int32_t{0});                       // LuID
    writeField(file_, format->colorId);                       // ColorID
    // The original specification has this flag reversed. Follow existing
    // readers: https://siril.readthedocs.io/en/latest/file-formats/SER.html
    writeField(file_, std::int32_t{0});                       // LittleEndian
    writeField(file_, static_cast<std::int32_t>(descriptor.width));
    writeField(file_, static_cast<std::int32_t>(descriptor.height));
    writeField(file_, static_cast<std::int32_t>(bits));

    frameCountFieldOffset_ = file_.tellp();
    writeField(file_, std::int32_t{0});  // FrameCount, patched in close()

    writeTextField(file_, kTextFieldSize);  // Observer
    writeTextField(file_, kTextFieldSize);  // Instrument
    writeTextField(file_, kTextFieldSize);  // Telescope

    const auto dateTimeTicks = unixNanosToDotNetUtcTicks(anchorSystemUnixNanos_);
    writeField(file_, dateTimeTicks);  // DateTime (local — using UTC, see header comment)
    writeField(file_, dateTimeTicks);  // DateTime_UTC

    if (!file_.good()) {
        return Result<void>::failure({ErrorCode::RecordingOpenFailed, "Failed writing header", path});
    }
    return Result<void>::success();
}

Result<void> SerRecorder::writeFrame(const FrameBuffer& frame) {
    if (!file_.is_open()) {
        return Result<void>::failure({ErrorCode::InternalInvariantViolation, "writeFrame() before open()", ""});
    }
    if (!frame.storage) {
        return Result<void>::failure({ErrorCode::InvalidFrame, "Frame has no backing storage", ""});
    }
    const auto& d = frame.descriptor;
    if (d.width != descriptor_.width || d.height != descriptor_.height || d.storage != descriptor_.storage ||
        d.bayer != descriptor_.bayer || d.validBits != descriptor_.validBits ||
        d.expectedPayloadBytes() != descriptor_.expectedPayloadBytes()) {
        return Result<void>::failure({ErrorCode::InvalidFrame, "Frame format changed during recording", ""});
    }
    if (framesWritten_ >= static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
        return Result<void>::failure({ErrorCode::DiskWriteFailed, "SER frame count limit reached", ""});
    }
    if (frame.storage->size() != descriptor_.expectedPayloadBytes()) {
        return Result<void>::failure(
            {ErrorCode::InvalidFrame, "Frame payload size does not match the recording's descriptor", ""});
    }

    // SER stores tightly packed rows. Camera buffers may have alignment
    // padding, which must not shift every subsequent row and timestamp.
    const auto rowBytes = static_cast<std::size_t>(descriptor_.width) * bytesPerPixel(descriptor_.storage);
    const auto stride = descriptor_.strideBytes != 0 ? descriptor_.strideBytes : rowBytes;
    const auto* data = reinterpret_cast<const char*>(frame.storage->data());
    if (stride == rowBytes) {
        file_.write(data, static_cast<std::streamsize>(rowBytes * descriptor_.height));
    } else {
        for (std::uint32_t y = 0; y < descriptor_.height; ++y) {
            file_.write(data + y * stride, static_cast<std::streamsize>(rowBytes));
        }
    }
    if (!file_.good()) {
        return Result<void>::failure({ErrorCode::DiskWriteFailed, "Failed writing frame", ""});
    }

    const auto frameSteadyNanos =
        std::chrono::duration_cast<std::chrono::nanoseconds>(frame.metadata.hostArrival.time_since_epoch()).count();
    const auto frameUnixNanos = anchorSystemUnixNanos_ + (frameSteadyNanos - anchorSteadyNanos_);
    frameTimestampsUtcTicks_.push_back(unixNanosToDotNetUtcTicks(frameUnixNanos));

    ++framesWritten_;
    return Result<void>::success();
}

Result<RecordingSummary> SerRecorder::close() {
    if (!file_.is_open()) {
        return Result<RecordingSummary>::success(RecordingSummary{framesWritten_});
    }

    // Trailer: one int64 UTC-ticks timestamp per frame, in capture order —
    // written before patching FrameCount so a crash mid-trailer-write still
    // leaves FrameCount at 0 (a reader treats that as "no valid frames"
    // rather than trusting a trailer that isn't actually complete).
    for (const auto ticks : frameTimestampsUtcTicks_) {
        writeField(file_, ticks);
    }
    const bool wroteTrailer = file_.good();

    file_.seekp(frameCountFieldOffset_);
    writeField(file_, static_cast<std::int32_t>(framesWritten_));
    const bool wroteFinalCount = file_.good();
    file_.close();

    if (!wroteTrailer || !wroteFinalCount || file_.fail()) {
        return Result<RecordingSummary>::failure({ErrorCode::DiskWriteFailed, "Failed finalizing recording", ""});
    }
    return Result<RecordingSummary>::success(RecordingSummary{framesWritten_});
}

}  // namespace astro::recording
