#pragma once

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
    };

    astro::domain::Result<Header> open(const std::string& path);
    astro::domain::Result<astro::domain::FrameBuffer> readNextFrame();

private:
    std::ifstream file_;
    astro::domain::FrameDescriptor descriptor_;
    std::uint64_t frameCount_ = 0;
    std::uint64_t framesRead_ = 0;
};

}  // namespace astro::recording
