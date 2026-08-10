#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "astro/recording/IRecorder.hpp"

namespace astro::recording {

// Writes AstroCapture's own placeholder raw-sequence format (magic
// "ACRAWSEQ") — deliberately NOT SER. A real SER writer needs the research
// spike IMPLEMENTATION_BLUEPRINT.md section 18 calls for (authoritative
// spec, golden files, cross-validation with established planetary
// software) before it's trustworthy; this exists so recording has *a*
// correct, testable, round-trippable implementation now rather than none.
// See RawSequenceReader for the matching reader and the on-disk layout.
class RawSequenceRecorder final : public IRecorder {
public:
    ~RawSequenceRecorder() override;

    astro::domain::Result<void> open(const std::string& path, const astro::domain::FrameDescriptor& descriptor,
                                      double nominalFps) override;
    astro::domain::Result<void> writeFrame(const astro::domain::FrameBuffer& frame) override;
    astro::domain::Result<RecordingSummary> close() override;

private:
    std::ofstream file_;
    astro::domain::FrameDescriptor descriptor_;
    std::uint64_t framesWritten_ = 0;
    std::streampos frameCountFieldOffset_{};
};

}  // namespace astro::recording
