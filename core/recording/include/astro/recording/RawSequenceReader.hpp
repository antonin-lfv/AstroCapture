#pragma once

#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>

#include "astro/domain/Frame.hpp"
#include "astro/domain/Result.hpp"

namespace astro::recording {

// Reads back files RawSequenceRecorder wrote. Exists to make the format
// round-trip-testable; deliberately not wired into any ICameraBackend — a
// replay backend needs the CLI/recording-driven workflow around this, not
// just a parser (see ROADMAP.md backlog).
class RawSequenceReader {
public:
    struct Header {
        std::uint32_t formatVersion = 0;
        astro::domain::FrameDescriptor descriptor;
        std::uint64_t frameCount = 0;
        double nominalFps = 0.0;
        std::int64_t anchorSteadyNanos = 0;
        std::int64_t anchorSystemUnixNanos = 0;
    };

    astro::domain::Result<Header> open(const std::string& path);
    astro::domain::Result<astro::domain::FrameBuffer> readNextFrame();
    astro::domain::Result<astro::domain::FrameBuffer> readFrame(std::uint64_t index);

private:
    std::ifstream file_;
    astro::domain::FrameDescriptor descriptor_;
    std::uint64_t frameCount_ = 0;
    std::uint64_t framesRead_ = 0;
    std::streamoff dataOffset_{};
};

// Recovers a frame's actual Unix epoch time in nanoseconds from its
// steady_clock hostArrival timestamp and the recording's two open()-time
// anchors, since steady_clock has no fixed epoch of its own and so isn't
// comparable across processes/machines on its own. Needed by per-frame
// derotation tools (e.g. WinJUPOS) that require real wall-clock timestamps,
// not just relative frame-to-frame offsets.
std::int64_t frameWallClockUnixNanos(const RawSequenceReader::Header& header,
                                      std::chrono::steady_clock::time_point frameHostArrival);

}  // namespace astro::recording
