#pragma once

#include <atomic>
#include <mutex>
#include <thread>

#include "astro/domain/ICameraDevice.hpp"

namespace astro::driver::mock {

// Simulated single-mode Bayer camera. Streams a static synthetic test
// pattern at a configurable rate from a dedicated worker thread, entirely
// in memory, so UI/pipeline/recording work never needs real hardware.
// Constructed only by MockCameraBackend::open().
class MockCameraDevice final : public astro::domain::ICameraDevice {
public:
    explicit MockCameraDevice(astro::domain::CameraDescriptor descriptor);
    ~MockCameraDevice() override;

    astro::domain::CameraDescriptor descriptor() const override;
    std::vector<astro::domain::StreamMode> streamModes() const override;
    std::vector<astro::domain::CameraCapability> capabilities() const override;

    astro::domain::Result<void> configure(const astro::domain::StreamConfiguration& configuration) override;
    astro::domain::Result<void> start(astro::domain::FrameSink& sink) override;
    astro::domain::Result<void> stop() override;

    astro::domain::Result<astro::domain::ControlValue> getControl(astro::domain::ControlId id) const override;
    astro::domain::Result<void> setControl(astro::domain::ControlId id,
                                            const astro::domain::ControlValue& value) override;

private:
    void runLoop();

    astro::domain::CameraDescriptor descriptor_;
    astro::domain::StreamMode supportedMode_;

    std::atomic<bool> configured_{false};
    astro::domain::StreamConfiguration configuration_{};

    std::atomic<bool> running_{false};
    std::thread worker_;
    astro::domain::FrameSink* sink_ = nullptr;
    std::atomic<std::uint64_t> sequence_{0};
    std::shared_ptr<const astro::domain::BufferStorage> testPattern_;

    mutable std::mutex controlsMutex_;
    double exposureMicroseconds_ = 10'000.0;
    std::int64_t gain_ = 0;
};

}  // namespace astro::driver::mock
