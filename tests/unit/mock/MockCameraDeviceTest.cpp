#include <chrono>
#include <thread>

#include "astro/driver/mock/MockCameraBackend.hpp"

#include "Check.hpp"

using namespace astro::domain;
using astro::driver::mock::MockCameraBackend;

namespace {

class CountingSink final : public FrameSink {
public:
    void onFrame(const FrameBuffer& frame) override {
        ++frameCount;
        lastDescriptor = frame.descriptor;
        lastSequence = frame.metadata.sequence;
        lastStorage = frame.storage;
    }

    int frameCount = 0;
    FrameDescriptor lastDescriptor{};
    std::uint64_t lastSequence = 0;
    std::shared_ptr<const BufferStorage> lastStorage;
};

}  // namespace

int main() {
    MockCameraBackend backend;
    ASTRO_TEST_CHECK(backend.id() == "mock");

    const auto descriptors = backend.enumerate();
    ASTRO_TEST_CHECK(descriptors.size() == 1);
    const CameraId deviceId = descriptors[0].id;

    ASTRO_TEST_CHECK(backend.open("does-not-exist") == nullptr);

    auto device = backend.open(deviceId);
    ASTRO_TEST_CHECK(device != nullptr);

    const auto modes = device->streamModes();
    ASTRO_TEST_CHECK(modes.size() == 1);
    ASTRO_TEST_CHECK(modes[0].width == 640 && modes[0].height == 480);
    ASTRO_TEST_CHECK(modes[0].storage == PixelStorage::Bayer8);
    ASTRO_TEST_CHECK(modes[0].bayer == BayerPattern::RGGB);

    // start() before configure() must fail.
    CountingSink sink;
    ASTRO_TEST_CHECK(!device->start(sink).ok());

    StreamConfiguration configuration;
    configuration.mode = modes[0];
    configuration.targetFps = 30.0;
    ASTRO_TEST_CHECK(device->configure(configuration).ok());

    ASTRO_TEST_CHECK(device->start(sink).ok());
    // Reconfiguring while streaming must be rejected.
    ASTRO_TEST_CHECK(!device->configure(configuration).ok());

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ASTRO_TEST_CHECK(device->stop().ok());

    ASTRO_TEST_CHECK(sink.frameCount > 0);
    ASTRO_TEST_CHECK(sink.lastDescriptor.width == 640 && sink.lastDescriptor.height == 480);
    ASTRO_TEST_CHECK(sink.lastDescriptor.storage == PixelStorage::Bayer8);
    ASTRO_TEST_CHECK(sink.lastDescriptor.bayer == BayerPattern::RGGB);
    ASTRO_TEST_CHECK(sink.lastSequence == static_cast<std::uint64_t>(sink.frameCount - 1));

    // stop() must be idempotent.
    ASTRO_TEST_CHECK(device->stop().ok());

    // ROI: a valid crop must only change the streamed frame's shape and
    // report exactly the sensor's sub-region content — verified against
    // the full-frame capture above at the corresponding offset, not just
    // by dimensions.
    ASTRO_TEST_CHECK(sink.lastStorage != nullptr);
    StreamConfiguration roiConfiguration = configuration;
    roiConfiguration.roi = Roi{2, 4, 8, 6};
    ASTRO_TEST_CHECK(device->configure(roiConfiguration).ok());

    CountingSink roiSink;
    ASTRO_TEST_CHECK(device->start(roiSink).ok());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASTRO_TEST_CHECK(device->stop().ok());

    ASTRO_TEST_CHECK(roiSink.frameCount > 0);
    ASTRO_TEST_CHECK(roiSink.lastDescriptor.width == 8 && roiSink.lastDescriptor.height == 6);
    ASTRO_TEST_CHECK(roiSink.lastDescriptor.bayer == BayerPattern::RGGB);
    ASTRO_TEST_CHECK(roiSink.lastStorage != nullptr);
    ASTRO_TEST_CHECK(roiSink.lastStorage->size() == 8 * 6);

    {
        const auto* fullBytes = reinterpret_cast<const std::uint8_t*>(sink.lastStorage->data());
        const auto* roiBytes = reinterpret_cast<const std::uint8_t*>(roiSink.lastStorage->data());
        for (std::uint32_t row = 0; row < 6; ++row) {
            for (std::uint32_t col = 0; col < 8; ++col) {
                const std::uint8_t expected = fullBytes[(4 + row) * 640 + (2 + col)];
                const std::uint8_t actual = roiBytes[row * 8 + col];
                ASTRO_TEST_CHECK(expected == actual);
            }
        }
    }

    // Invalid ROI (odd origin, or out of bounds) must be rejected while
    // stopped, never silently rounded or clamped.
    StreamConfiguration oddOrigin = configuration;
    oddOrigin.roi = Roi{1, 0, 8, 6};
    ASTRO_TEST_CHECK(!device->configure(oddOrigin).ok());

    StreamConfiguration outOfBounds = configuration;
    outOfBounds.roi = Roi{636, 0, 8, 6};
    ASTRO_TEST_CHECK(!device->configure(outOfBounds).ok());

    // Control round-trip.
    auto exposure = device->getControl(ControlId::Exposure);
    ASTRO_TEST_CHECK(exposure.ok());
    ASTRO_TEST_CHECK(std::get<double>(exposure.value()) == 10'000.0);

    ASTRO_TEST_CHECK(device->setControl(ControlId::Exposure, ControlValue{5'000.0}).ok());
    ASTRO_TEST_CHECK(std::get<double>(device->getControl(ControlId::Exposure).value()) == 5'000.0);

    // Wrong value type for the control must be rejected, not silently coerced.
    ASTRO_TEST_CHECK(!device->setControl(ControlId::Exposure, ControlValue{std::string("bad")}).ok());

    // Unsupported control.
    ASTRO_TEST_CHECK(!device->getControl(ControlId::Temperature).ok());
    ASTRO_TEST_CHECK(!device->setControl(ControlId::Temperature, ControlValue{std::int64_t{0}}).ok());

    // Gate D backlog 22.3: gamma/white balance/brightness/frame rate limit
    // round-trip the same way exposure/gain do above.
    ASTRO_TEST_CHECK(device->setControl(ControlId::Gamma, ControlValue{1.8}).ok());
    ASTRO_TEST_CHECK(std::get<double>(device->getControl(ControlId::Gamma).value()) == 1.8);
    ASTRO_TEST_CHECK(!device->setControl(ControlId::Gamma, ControlValue{10.0}).ok());

    ASTRO_TEST_CHECK(device->setControl(ControlId::WhiteBalanceRed, ControlValue{std::int64_t{70}}).ok());
    ASTRO_TEST_CHECK(std::get<std::int64_t>(device->getControl(ControlId::WhiteBalanceRed).value()) == 70);
    ASTRO_TEST_CHECK(device->setControl(ControlId::WhiteBalanceBlue, ControlValue{std::int64_t{30}}).ok());
    ASTRO_TEST_CHECK(std::get<std::int64_t>(device->getControl(ControlId::WhiteBalanceBlue).value()) == 30);
    ASTRO_TEST_CHECK(!device->setControl(ControlId::WhiteBalanceRed, ControlValue{std::int64_t{200}}).ok());

    ASTRO_TEST_CHECK(device->setControl(ControlId::Brightness, ControlValue{std::int64_t{40}}).ok());
    ASTRO_TEST_CHECK(std::get<std::int64_t>(device->getControl(ControlId::Brightness).value()) == 40);
    ASTRO_TEST_CHECK(!device->setControl(ControlId::Brightness, ControlValue{std::int64_t{-1}}).ok());

    ASTRO_TEST_CHECK(device->setControl(ControlId::FrameRateLimit, ControlValue{45.0}).ok());
    ASTRO_TEST_CHECK(std::get<double>(device->getControl(ControlId::FrameRateLimit).value()) == 45.0);
    ASTRO_TEST_CHECK(!device->setControl(ControlId::FrameRateLimit, ControlValue{500.0}).ok());

    std::cout << "OK\n";
    return 0;
}
