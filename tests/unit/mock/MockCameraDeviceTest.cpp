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
    }

    int frameCount = 0;
    FrameDescriptor lastDescriptor{};
    std::uint64_t lastSequence = 0;
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

    std::cout << "OK\n";
    return 0;
}
