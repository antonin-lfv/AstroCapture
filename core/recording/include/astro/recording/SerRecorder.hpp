#pragma once

#include <cstdint>
#include <fstream>
#include <streambuf>
#include <string>
#include <vector>

#include "astro/recording/IRecorder.hpp"

namespace astro::recording {

// Writes the SER planetary-video format (Gate D backlog 22.9 / blueprint
// section 18): the de facto standard for planetary capture, readable by
// AutoStakkert, PIPP, Registax and FireCapture itself. Implemented against
// the SER format's public specification (178-byte fixed header, raw frame
// data, optional per-frame UTC timestamp trailer — unchanged since its
// introduction and implemented identically by every tool named above), not
// against a real reference file, since none was available to cross-
// validate against in this environment; RawSequenceRecorder remains the
// fallback for formats SER has no color ID for (see serColorIdFor in the
// .cpp). Frame color/mono handling: Bayer sources are written as raw,
// un-demosaiced single-plane data with the matching Bayer color ID — SER
// readers debayer on load, the same way this project's own preview does.
class SerRecorder final : public IRecorder {
public:
    ~SerRecorder() override;

    astro::domain::Result<void> open(const std::string& path, const astro::domain::FrameDescriptor& descriptor,
                                      double nominalFps) override;
    astro::domain::Result<void> writeFrame(const astro::domain::FrameBuffer& frame) override;
    astro::domain::Result<RecordingSummary> close() override;

private:
    std::ofstream file_;
    astro::domain::FrameDescriptor descriptor_;
    std::uint64_t framesWritten_ = 0;
    std::streampos frameCountFieldOffset_{};

    // SER has no fixed-epoch clock of its own to stamp frames with as they
    // arrive (see RawSequenceRecorder's identical problem/fix): these
    // anchors, taken back-to-back at open(), let writeFrame() convert each
    // frame's steady_clock hostArrival into an absolute .NET-ticks UTC
    // timestamp for the trailer.
    std::int64_t anchorSteadyNanos_ = 0;
    std::int64_t anchorSystemUnixNanos_ = 0;
    std::vector<std::int64_t> frameTimestampsUtcTicks_;
};

}  // namespace astro::recording
