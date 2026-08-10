#pragma once

#include <cstdint>
#include <string>

#include "astro/domain/Frame.hpp"
#include "astro/domain/Result.hpp"

namespace astro::recording {

struct RecordingSummary {
    std::uint64_t framesWritten = 0;
};

// Recording file-format details never leak into capture orchestration
// (IMPLEMENTATION_BLUEPRINT.md section 18): callers only see open/write/
// close. All frames passed to writeFrame() between open() and close() must
// share open()'s descriptor — implementations are not required to support
// resolution/format changes mid-recording, since the current capture
// pipeline can't produce that anyway (stream configuration is fixed before
// start()).
class IRecorder {
public:
    virtual ~IRecorder() = default;

    virtual astro::domain::Result<void> open(const std::string& path,
                                              const astro::domain::FrameDescriptor& descriptor,
                                              double nominalFps) = 0;

    virtual astro::domain::Result<void> writeFrame(const astro::domain::FrameBuffer& frame) = 0;

    virtual astro::domain::Result<RecordingSummary> close() = 0;
};

}  // namespace astro::recording
