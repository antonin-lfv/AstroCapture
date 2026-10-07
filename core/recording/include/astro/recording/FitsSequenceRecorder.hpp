#pragma once
#include <astro/recording/IRecorder.hpp>
#include <astro/recording/AstroImage.hpp>
namespace astro::recording {
class FitsSequenceRecorder final : public IRecorder {
public:
    AstroImage metadata;
    domain::Result<void> open(const std::string&, const domain::FrameDescriptor&, double) override;
    domain::Result<void> writeFrame(const domain::FrameBuffer&) override;
    domain::Result<RecordingSummary> close() override;
private:
    std::filesystem::path directory_;
    domain::FrameDescriptor descriptor_;
    std::uint64_t count_{};
};
}
